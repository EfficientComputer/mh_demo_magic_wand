import { useEffect, useState, useRef } from 'react';
import { apiService } from '../services/api';

interface HoldStillProps {
  gameId: string;
  onStillnessDetected: () => void;
  onError: (message: string) => void;
}

function HoldStill({ gameId, onStillnessDetected, onError }: HoldStillProps) {
  const [status, setStatus] = useState<'waiting' | 'sending' | 'ready'>('waiting');
  const hasRun = useRef(false);

  useEffect(() => {
    if (hasRun.current) return;
    hasRun.current = true;

    const handleStillness = async () => {
      try {
        // 1) Wait for stillness/ready via 0xF7
        setStatus('waiting');
        const res = await apiService.waitForReady(gameId);
        if (res.status !== 'ready') {
          console.warn('Stillness wait timed out. Restarting game.');
          onError('Stillness not detected. Restarting...');
          return;
        }

        // 2) Nano is in ready/stillness; send GO to arm the capture
        setStatus('sending');
        const goRes = await apiService.sendGo(gameId);
        if (goRes.status !== 'ok') {
          console.warn('GO command not acknowledged. Restarting game.');
          onError('Unable to start next gesture. Restarting...');
          return;
        }

        // 3) Ready and armed
        setStatus('ready');
        onStillnessDetected();
      } catch (e) {
        console.error('Failed while waiting for stillness or sending GO:', e);
        onError('Failed while preparing gesture.');
      }
    };

    handleStillness();
  }, [gameId, onStillnessDetected, onError]);

  return (
    <section className="text-center w-full">
      <h1 className="font-semibold m-0 mb-4 tracking-wide text-[clamp(2.4rem,4vw,3.4rem)]">
        Hold the Wand Still!
      </h1>
      <div className="w-[250px] h-[250px] my-6 mx-auto relative flex items-center justify-center">
        <div className="w-32 h-32 border-8 border-[var(--color-border)] border-t-[var(--color-accent-gold)] rounded-full animate-spin"></div>
      </div>
      <p className="text-xl mt-2" style={{ color: 'var(--color-text-dim)' }}>
        {status === 'waiting' && 'Stabilizing sensors...'}
        {status === 'sending' && 'Arming capture...'}
        {status === 'ready' && 'Ready!'}
      </p>
    </section>
  );
}

export default HoldStill;

