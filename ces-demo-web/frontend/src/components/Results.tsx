import { useMemo } from "react";
import { GestureChars, getGestureImage } from "../models/Gestures";
import type { GameResults } from "../models/GameResults";

interface ResultsProps {
  results: GameResults | undefined;
  onContinue: () => void;
}

function Results({ results, onContinue }: ResultsProps) {
  const score = useMemo(() => {
    if (!results?.gestures || results.gestures.length === 0) return { percentage: 0, rating: 'Unknown' };
    const correctCount = results.gestures.filter(gesture => gesture.received === gesture.original).length;
    const percentage = Math.round((correctCount / results.gestures.length) * 100);

    let rating: string;
    if (percentage <= 25) {
      rating = 'Poor';
    } else if (percentage <= 50) {
      rating = 'Fair';
    } else if (percentage <= 75) {
      rating = 'Good';
    } else {
      rating = 'Excellent';
    }

    return { percentage, rating };
  }, [results]);

  return (
    <section className="w-full">
      <h1 className="font-semibold m-0 mb-6">Results</h1>
      <div className="flex flex-wrap items-center gap-6">
        <div className="flex-[1_1_260px] bg-[var(--color-bg)] border border-[var(--color-border)] rounded-md p-6 px-7 relative shadow-[0_4px_14px_rgba(0,0,0,0.35)] text-center">
          <h3 className="m-0 mb-3 text-base tracking-wide uppercase text-[var(--color-text-dim)]">Success</h3>
          <div className="text-[2.8rem] font-bold leading-none text-[var(--color-accent-gold)]" id="successPct">
            {score.percentage}%
          </div>
          <div className="text-xl mt-1.5 text-[var(--color-text-dim)]">Gestures correct</div>
        </div>
        <div className="flex-[1_1_260px] bg-[var(--color-bg)] border border-[var(--color-border)] rounded-md p-9 px-7 relative shadow-[0_4px_14px_rgba(0,0,0,0.35)] text-center">
          <h3 className="m-0 mb-3 text-base tracking-wide uppercase text-[var(--color-text-dim)]">Rating</h3>
          <div
            className="inline-block py-2.5 px-5 rounded-full border border-[var(--color-border)] font-semibold tracking-wide text-xl text-[var(--color-accent-gold)] bg-[var(--color-panel)]"
            id="ratingText"
          >
            {score.rating}
          </div>
        </div>
      </div>
      <h1 className="font-semibold mt-16 mb-6">Gesture Outcomes</h1>
      <div className="grid gap-7 grid-cols-[repeat(auto-fit,minmax(160px,1fr))] mb-10" id="resultsGrid">
        {results?.gestures.map((gesture, index) => {
          const receivedCharacter = GestureChars[gesture.received];
          const originalCharacter = GestureChars[gesture.original];

          return (
            <div
              key={index}
              className="bg-[var(--color-bg)] border rounded-md relative p-2 flex flex-col items-center justify-center shadow-[0_4px_14px_rgba(0,0,0,0.35)] border-[var(--color-border)]"
              aria-label={`Gesture ${originalCharacter}: ${receivedCharacter === originalCharacter ? 'success' : 'failure'}`}
            >
              <div
                className="w-full aspect-square bg-[var(--color-panel)] rounded-sm flex flex-col items-center justify-between text-[2rem] text-[var(--color-text-dim)]"
              >
                <div className="flex-1 flex items-center justify-center pt-4 w-full">
                  <img
                    src={getGestureImage(gesture.received)}
                    alt={receivedCharacter}
                    className="w-full h-full object-contain p-4"
                  />
                </div>
              </div>
              {receivedCharacter === originalCharacter ? (
                <div
                  className="absolute top-2.5 right-2.5 w-[42px] h-[42px] rounded-full flex items-center justify-center text-[1.4rem] font-bold shadow-[0_0_0_3px_rgba(0,0,0,0.35)] text-[var(--color-bg)] bg-[var(--color-success)]"
                >
                  ✓
                </div>
              ) : (
                <div
                  className="absolute top-2.5 right-2.5 w-[42px] h-[42px] rounded-full flex items-center justify-center text-[1.4rem] font-bold shadow-[0_0_0_3px_rgba(0,0,0,0.35)] text-[var(--color-bg)] bg-[var(--color-error)]"
                >
                  ✗
                </div>
              )}
              <div
                className="flex items-center justify-center text-[var(--color-text-dim)]"
                style={{
                  marginTop: originalCharacter === '∠' ? '0rem' : '0.5rem'
                }}
              >
                <span style={{ fontSize: originalCharacter === '∠' ? '2rem' : '1.5rem' }}>
                  {originalCharacter}
                </span>
              </div>
            </div>
          )
        })}
      </div>
      <div className="text-center mt-10">
        <button className="btn" id="returnHomeBtn" onClick={onContinue}>Return to Home</button>
      </div>
    </section>
  );
}

export default Results;
