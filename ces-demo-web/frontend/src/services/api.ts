import axios from 'axios';
import type { AxiosInstance } from 'axios';

// API Service for BLE Backend
const API_BASE_URL = 'http://localhost:8000';

// Type definitions for API responses
export type GameStatus = 'listening' | 'waiting' | 'complete' | 'error';

export interface StartGameResponse {
  game_id: string;
}

export interface ReadyGameResponse {
  sent: number[];
  delay: number;
  status: GameStatus;
  game_id: string;
}

export interface GestureProgressResponse {
  status: 'waiting' | 'in-progress';
  game_id: string;
  sent: number[] | null;
  progress: number[];
  next_index: number;
  error?: string;
}

export interface FinalResultsResponse {
  status: 'complete';
  game_id: string;
  sent: number[] | null;
  progress: number[];
  received: {
    power_usage: number;
    gestures: number[];
  };
}

export interface GameResultsResponse {
  status: GameStatus | 'in-progress';
  game_id: string;
  sent: number[] | null;
  progress?: number[];
  next_index?: number;
  received: {
    power_usage: number;
    gestures: number[][] | number[];
  } | null;
  error?: string;
}

// API Service class
class ApiService {
  private client: AxiosInstance;

  constructor(baseUrl: string = API_BASE_URL) {
    this.client = axios.create({
      baseURL: baseUrl,
      headers: {
        'Content-Type': 'application/json',
      },
    });
  }

  /**
   * Get the BLE connection status
   */
  async getConnectionStatus(): Promise<boolean> {
    const response = await this.client.get<boolean>('/api/game/connection-status');
    return response.data;
  }

  /**
   * Start a new game and get the gesture sequence
   */
  async startNewGame(): Promise<StartGameResponse> {
    const response = await this.client.post<{ game_id: string }>('/api/game/start');
    return response.data;
  }

  /** 
   * Send gestures to Arduino and start listening for response
   */
  async sendGestures(gameId: string): Promise<ReadyGameResponse> {
    const response = await this.client.post<ReadyGameResponse>(`/api/game/ready`, { game_id: gameId });
    return response.data;
  }

  /**
   * Poll for gesture progress updates during gameplay
   */
  async pollGestureProgress(gameId: string): Promise<GestureProgressResponse> {
    const response = await this.client.get<GameResultsResponse>(`/api/game/results/${gameId}`);
    return {
      status: response.data.status as 'waiting' | 'in-progress',
      game_id: response.data.game_id,
      sent: response.data.sent,
      progress: response.data.progress ?? [],
      next_index: response.data.next_index ?? 0,
      error: response.data.error
    };
  }

  /**
   * Get final game results with power usage
   */
  async getFinalResults(gameId: string): Promise<FinalResultsResponse> {
    const response = await this.client.get<GameResultsResponse>(`/api/game/results/${gameId}`);
    if (response.data.status !== 'complete' || !response.data.received) {
      throw new Error(`Results not ready: status=${response.data.status}`);
    }
    return {
      status: 'complete',
      game_id: response.data.game_id,
      sent: response.data.sent,
      progress: response.data.progress ?? [],
      received: response.data.received
    };
  }

  /**
   * Get the game results (legacy - use pollGestureProgress or getFinalResults instead)
   */
  async getGameResults(gameId: string): Promise<GameResultsResponse> {
    const response = await this.client.get<GameResultsResponse>(`/api/game/results/${gameId}`);
    return response.data;
  }

  /**
   * Block until the Nano reports stillness/ready via 0xF7.
   */
  async waitForReady(gameId: string): Promise<{ game_id: string; status: 'ready' | 'timeout' }> {
    const response = await this.client.get<{ game_id: string; status: 'ready' | 'timeout' }>(`/api/game/wait-ready/${gameId}`);
    return response.data;
  }

  /**
   * Send a GO command to arm the next gesture capture.
   */
  async sendGo(gameId: string): Promise<{ game_id: string; status: 'ok' }> {
    const response = await this.client.post<{ game_id: string; status: 'ok' }>(`/api/game/go/${gameId}`);
    return response.data;
  }
}

// Export a singleton instance
export const apiService = new ApiService();

// Export the class for testing or custom instances
export default ApiService;
