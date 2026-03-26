import { useEffect, useMemo, useState } from 'react';
import Start from '../../components/Start';
import Instructions from '../../components/Instructions';
import Countdown from '../../components/Countdown';
import { apiService } from '../../services/api';
import Gesture from '../../components/Gesture';
import HoldStill from '../../components/HoldStill';
import Results from '../../components/Results';
import type { GameResults } from '../../models/GameResults';
import { pollApi } from '../../utils/polling';

function Home() {
  const steps = useMemo(() => [
    'Start',
    'Instructions',
    'CountDown',
    'Gesture Display',
    'Hold Still',
    'Results',
  ], []);
  const [currentStepIndex, setCurrentStepIndex] = useState(0);
  const [gameId, setGameId] = useState<string | undefined>();
  const [gestures, setGestures] = useState<number[]>([]);
  const [currentGestureIndex, setCurrentGestureIndex] = useState<number | undefined>();
  const [results, setResults] = useState<GameResults | undefined>();
  const [wandConnected, setWandConnected] = useState(false);
  const [isLoadingResults, setIsLoadingResults] = useState(false);
  const [errorMessage, setErrorMessage] = useState<string | null>(null);

  useEffect(() => {
    const checkConnectionStatus = async () => {
      try {
        const isConnected = await apiService.getConnectionStatus();
        setWandConnected(isConnected);
      } catch (error) {
        console.error('Failed to get connection status:', error);
        setWandConnected(false);
      }
    };

    // Check immediately on mount
    checkConnectionStatus();
    // Poll every 2 seconds to keep status updated
    const interval = setInterval(checkConnectionStatus, 2000);

    return () => clearInterval(interval);
  }, []);

  const handleAdvanceGame = async () => {
    // Countdown gates the very first gesture
    if (currentStepIndex === 2) {
      // Advance to HoldStill, which will handle waiting for ready + sending GO
      setCurrentStepIndex(4);
      return;
    }

    // After showing a gesture (coming from Gesture component)
    if (currentStepIndex === 3) {
      // Check if we just showed the last gesture
      const justShowedLastGesture =
        currentGestureIndex !== undefined &&
        currentGestureIndex >= gestures.length - 1;

      if (justShowedLastGesture) {
        // We've shown all gestures, proceed to results
        setIsLoadingResults(true);
        setCurrentStepIndex(5);
        await requestResults();
        return;
      }

      // More gestures to show - increment index and go to HoldStill
      setCurrentGestureIndex((prevIndex) => (prevIndex !== undefined ? prevIndex + 1 : 0));
      setCurrentStepIndex(4);
      return;
    }

    // Default advance for any other step
    setCurrentStepIndex((prevIndex) => prevIndex >= steps.length - 1 ? 0 : prevIndex + 1);
  };

  const startGame = async () => {
    try {
      setGestures([]);
      setCurrentGestureIndex(undefined);
      setResults(undefined);
      const start = await apiService.startNewGame();
      setGameId(start.game_id);
      // Advance from Start to Instructions; the Instructions screen
      // will trigger gesture generation (/api/game/ready) and then
      // move into the initial countdown.
      setCurrentStepIndex(1);


    } catch (error) {
      console.error('Failed to start new game:', error);
      // TODO: display error popup
    }
  };

  const restartGameFlow = async () => {
    try {
      setCurrentStepIndex(0);
      setGestures([]);
      setCurrentGestureIndex(undefined);
      setResults(undefined);
      setIsLoadingResults(false);
      setErrorMessage(null);

    } catch (e) {

      console.error('Failed to restart game flow:', e);
      setErrorMessage('Unable to restart game. Check connection.');
    }
  };

  const handleStillnessDetected = () => {
    // HoldStill completed: go to Gesture display
    // The gesture index should already be set correctly before entering HoldStill
    setCurrentStepIndex(3); // Go to Gesture display
  };

  const requestGestures = async () => {
    if (!gameId) {
      setErrorMessage('No game ID available');
      return;
    }
    setErrorMessage(null);
    try {
      const ready = await pollApi({
        action: () => apiService.sendGestures(gameId),
        timeoutMs: 30000,
        onTimeout: async () => {
          console.warn('Gesture send exceeded 30s. Restart game.');
          setErrorMessage('Game timed out waiting for a response.');
        },
        on503Retry: () => setErrorMessage('Device disconnected. Retrying...')
      });
      if (!ready) return; // timeout handled
      // Successful non-503 response; clear any lingering error.
      setErrorMessage(null);
      setGestures(ready.sent);
      setCurrentGestureIndex(0);
      handleAdvanceGame();
    } catch (e) {
      console.error('Failed to send gestures:', e);
      setErrorMessage('Failed to send gestures.');
    }
  };

  const requestResults = async () => {
    if (!gameId) {
      setErrorMessage('No game ID available');
      return;
    }
    setIsLoadingResults(true);
    setErrorMessage(null);
    try {
      const final = await pollApi({
        action: async () => {
          try {
            return await apiService.getFinalResults(gameId);
          } catch (e) {
            // If not ready, return a sentinel to continue polling
            return null as any;
          }
        },
        shouldContinue: (r) => r === null,
        intervalMs: 1000,
        timeoutMs: 30000,
        onTimeout: async () => {
          console.warn('Results polling timeout. Restart game.');
          setErrorMessage('Game timed out waiting for a response.');
        },
        on503Retry: () => setErrorMessage('Device disconnected. Waiting to reconnect...'),
        onSuccess: () => setErrorMessage(null)
      });
      if (!final) return; // timeout handled
      if ((final as any).status === 'error') {
        const err = final as any;
        throw new Error(err.error || 'Unknown result error');
      }
      if (final.status === 'complete') {
        const receivedGestures: number[] = final.received?.gestures ?? [];

        const gameResults: GameResults = {
          gestures: receivedGestures.map((scores, idx) => ({
            original: final.sent?.[idx] ?? 3,
            received: scores,
          })),
          power_usage: final.received?.power_usage ?? 0,
        };
        setResults(gameResults);
      }

    } catch (e) {
      console.error('Failed to get game results:', e);
      setErrorMessage('Failed to get results.');
    } finally {
      setIsLoadingResults(false);
    }
  };

  return (
    <main className="min-h-screen w-full flex flex-col items-center justify-center p-8" id="main">
      <div className="w-full max-w-6xl flex flex-col gap-8">
        <header className="w-full text-center">
          <img src="/src/assets/title-logo.svg" alt="Efficient Computer" className="m-0 mx-auto h-32" />
          <div className="text-base tracking-wide uppercase text-lg mt-[-16px] text-[var(--color-text-dim)]">
            Magic Wand Demo • CES 2026
          </div>
        </header>

        <div className="w-full border rounded-lg p-12 relative flex-1 flex flex-col items-center justify-center bg-[var(--color-panel)] border-[var(--color-border)] shadow-[var(--shadow-soft)]">
          {errorMessage && (
            <div className="absolute inset-0 bg-black/60 flex flex-col items-center justify-center gap-4 p-8 z-10">
              <div className="text-xl font-semibold text-[var(--color-text)] text-center">{errorMessage}</div>
              <button
                onClick={() => restartGameFlow()}
                className="px-6 py-2 rounded bg-[var(--color-accent-gold)] text-[var(--color-bg)] font-medium shadow"
              >Restart</button>
            </div>
          )}
          <div className="w-full">
            {currentStepIndex === 0 && <Start isWandConnected={wandConnected} onStart={startGame} />}
            {currentStepIndex === 1 && <Instructions onContinue={requestGestures} />}
            {currentStepIndex === 2 && (
              <Countdown
                onContinue={handleAdvanceGame}
                gestures={gestures}
                gestureIndex={currentGestureIndex}
              />
            )}

            {currentStepIndex === 3 && (
              <Gesture
                gestureIndex={currentGestureIndex}
                gestures={gestures}
                onContinue={handleAdvanceGame}
              />
            )}
            {currentStepIndex === 4 && gameId && (
              <HoldStill
                gameId={gameId}
                onStillnessDetected={handleStillnessDetected}
                onError={(message) => {
                  setErrorMessage(message);
                }}
              />
            )}
            {currentStepIndex === 5 && isLoadingResults && (
              <div className="flex flex-col items-center justify-center gap-4">
                <div className="w-16 h-16 border-4 border-t-[var(--color-accent-gold)] border-[var(--color-border)] rounded-full animate-spin"></div>
                <div className="text-xl text-[var(--color-text-dim)]">Processing results...</div>
              </div>
            )}
            {currentStepIndex === 5 && !isLoadingResults && results && <Results results={results} onContinue={handleAdvanceGame} />}
          </div>
        </div>

        <footer className="text-center text-md text-[var(--color-text-dim)]">
          &copy; 2026 Efficient Computer – Trade Show Demo
        </footer>
      </div>
    </main>
  );
}

export default Home;
