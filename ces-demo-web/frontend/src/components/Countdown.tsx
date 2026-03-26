import { useState, useEffect } from 'react';

interface CountdownProps {
  gestureIndex?: number;
  gestures?: number[];
  onContinue: () => void;
}

function Countdown({ gestureIndex, gestures, onContinue }: CountdownProps) {
  const [timeLeft, setTimeLeft] = useState(4);
  const [progress, setProgress] = useState(0);

  useEffect(() => {
    // Reset timer when gesture changes
    setTimeLeft(4);
    setProgress(0);

    const startTime = Date.now();
    const duration = 4000;
    const progressDuration = 3000;

    const timer = setInterval(() => {
      const elapsed = Date.now() - startTime;
      const remaining = Math.max(0, duration - elapsed);
      const newTimeLeft = Math.ceil(remaining / 1000);
      const newProgress = Math.min(100, (elapsed / progressDuration) * 100);

      setTimeLeft(newTimeLeft);
      setProgress(newProgress);

      if (remaining <= 0) {
        clearInterval(timer);
        onContinue();
      }
    }, 50); // Update every 50ms for smooth animation

    return () => clearInterval(timer);
    // Depend only on gestureIndex so parent re-renders
    // (e.g. connection or ready-state updates) don't reset
    // the countdown once it has started.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [gestureIndex]);

  return (
    <section className="w-full">
      <div className="flex justify-between items-center gap-8 mb-6">
        <div className="font-semibold tracking-wide text-lg">
          Gesture <span id="gestureIndex">{(gestureIndex ?? 0) + 1}</span> of <span id="gestureTotal">{gestures?.length ?? 1}</span>
        </div>
        <div className="min-w-[90px] text-right font-semibold text-lg" id="timeRemaining">
          {Math.max(timeLeft - 1, 0)}
        </div>
      </div>

      <h1 className="text-center mt-10">Get Ready</h1>

      <div className="max-w-[800px] mx-auto my-9">
        <div className="flex-1 h-3.5 rounded-md overflow-hidden relative bg-[var(--color-border)]">
          <div
            className="bar-inner h-full transition-all duration-1000 ease-linear"
            style={{
              width: `${progress}%`,
              background: 'linear-gradient(145deg, var(--color-accent-gold), var(--color-accent-gold-soft))',
            }}
          ></div>
        </div>
      </div>

      <div className="text-center text-[4.5rem] font-bold" style={{ color: 'var(--color-accent-gold)' }} id="countNumber">
        {Math.max(timeLeft - 1, 0)}
      </div>

      <p className="text-center mt-8 text-xl" style={{ color: 'var(--color-text-dim)' }}>
        Hold the wand still and prepare for your first gesture.
      </p>
    </section>
  );
}

export default Countdown;
