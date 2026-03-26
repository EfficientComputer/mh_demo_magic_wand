import { useState, useEffect } from 'react';
import { GestureChars, getGestureImage, GestureImages } from '../models/Gestures';

interface GestureProps {
  gestureIndex?: number;
  gestures: number[];
  onContinue: () => void;
}

function Gesture({ gestureIndex, gestures, onContinue }: GestureProps) {
  const [timeLeft, setTimeLeft] = useState(5);
  const [progress, setProgress] = useState(0);

  useEffect(() => {
    // Reset timer when gesture changes
    setTimeLeft(5);
    setProgress(0);

    const startTime = Date.now();
    const duration = 5000; // 5 seconds in milliseconds

    const timer = setInterval(() => {
      const elapsed = Date.now() - startTime;
      const remaining = Math.max(0, duration - elapsed);
      const newTimeLeft = Math.ceil(remaining / 1000);
      const newProgress = Math.min(100, (elapsed / duration) * 100);

      setTimeLeft(newTimeLeft);
      setProgress(newProgress);

      if (remaining <= 0) {
        clearInterval(timer);
        onContinue();
      }
    }, 50); // Update every 50ms for smooth animation

    return () => clearInterval(timer);
  }, [gestureIndex, onContinue]);

  return (
    <section >
      <div className="flex justify-between items-center gap-8 mb-6">
        <div className="font-semibold tracking-wide text-lg">
          Gesture <span id="gestureIndex">{gestureIndex !== undefined ? gestureIndex + 1 : '?'}</span> of <span id="gestureTotal">{gestures.length}</span>
        </div>
        <div className="flex-1 bg-[var(--color-border)] h-3.5 rounded-md overflow-hidden relative">
          <div
            className="h-full transition-all duration-75 ease-linear"
            style={{
              background: 'linear-gradient(90deg, var(--color-accent-gold) 0%, var(--color-accent-gold-soft) 100%)',
              width: `${progress}%`
            }}
          />
        </div>
        <div className="min-w-[90px] text-right font-semibold text-lg" id="timeLeft">
          {timeLeft}s
        </div>
      </div>
      <div
        className="w-[400px] aspect-square rounded-lg bg-[var(--color-bg)] mx-auto mb-8 flex items-center justify-center border-2 border-[var(--color-border)] shadow-[0_10px_28px_rgba(0,0,0,0.45)]"
        id="gestureImage"
      >
        {gestureIndex !== undefined ? (
          <img
            src={getGestureImage(gestures[gestureIndex])}
            alt={GestureChars[gestures[gestureIndex]]}
            className={`w-full h-full object-contain p-4}`}
          />
        ) : (
          <img
            src={GestureImages[3]}
            alt="Unknown"
            className="w-full h-full object-contain p-8"
          />
        )}
      </div>
      <div
        className="text-center text-xl mb-6"
        style={{
          color: 'var(--color-text-dim)',
          marginTop: gestureIndex !== undefined && GestureChars[gestures[gestureIndex]] === '∠' ? '-1rem' : '-0.5rem'
        }}
        id="gestureName"
      >
        Gesture: {gestureIndex !== undefined ? (
          <span style={{ fontSize: GestureChars[gestures[gestureIndex]] === '∠' ? '2rem' : 'inherit' }}>
            {GestureChars[gestures[gestureIndex]]}
          </span>
        ) : '?'}
      </div>
      <h2 className="text-center mb-3">Perform this gesture now!</h2>
      <p className="text-center text-xl text-[var(--color-text-dim)]">
        Keep motion fluid; orientation matters.
      </p>
    </section>
  );
}

export default Gesture;
