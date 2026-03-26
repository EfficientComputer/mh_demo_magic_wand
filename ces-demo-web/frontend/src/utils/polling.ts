/**
 * Reusable polling helper with 503 retry, timeout, and optional continuation condition.
 */
export async function pollApi<T>(options: {
  action: () => Promise<T>;
  shouldContinue?: (result: T) => boolean;
  intervalMs?: number;
  timeoutMs?: number;
  onTimeout?: () => Promise<void> | void;
  on503Retry?: () => void;
  onSuccess?: (result: T) => void; // called on every successful non-503 response (even if continuing)
}): Promise<T | undefined> {
  const { action, shouldContinue, intervalMs = 1000, timeoutMs = 30000, onTimeout, on503Retry, onSuccess } = options;
  const start = Date.now();
  
  while (true) {
    try {
      const res = await action();
      if (onSuccess) onSuccess(res);
      if (shouldContinue && shouldContinue(res)) {
        if (Date.now() - start > timeoutMs) {
          if (onTimeout) await onTimeout();
          return undefined;
        }
        await new Promise(r => setTimeout(r, intervalMs));
        continue;
      }
      return res;
    } catch (err: any) {
      const status = err?.response?.status;
      if (status === 503) {
        if (Date.now() - start > timeoutMs) {
          if (onTimeout) await onTimeout();
          return undefined;
        }
        if (on503Retry) on503Retry();
        await new Promise(r => setTimeout(r, intervalMs));
        continue;
      }
      throw err;
    }
  }
}
