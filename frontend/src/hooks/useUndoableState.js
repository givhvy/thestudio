import { useState, useRef, useCallback } from 'react';

// useUndoableState — drop-in replacement for useState that records a history.
// Call setStateUndoable(next, { commit: true }) to push a history entry.
// Returns: [state, setState, { undo, redo, canUndo, canRedo, clear }]
export function useUndoableState(initial, { limit = 100 } = {}) {
  const [state, setStateRaw] = useState(initial);
  const past = useRef([]);
  const future = useRef([]);

  const setState = useCallback((next, opts = {}) => {
    setStateRaw(prev => {
      const value = typeof next === 'function' ? next(prev) : next;
      if (opts.commit !== false) {
        past.current.push(prev);
        if (past.current.length > limit) past.current.shift();
        future.current = [];
      }
      return value;
    });
  }, [limit]);

  const undo = useCallback(() => {
    setStateRaw(prev => {
      if (!past.current.length) return prev;
      future.current.push(prev);
      return past.current.pop();
    });
  }, []);

  const redo = useCallback(() => {
    setStateRaw(prev => {
      if (!future.current.length) return prev;
      past.current.push(prev);
      return future.current.pop();
    });
  }, []);

  const clear = useCallback(() => {
    past.current = [];
    future.current = [];
  }, []);

  return [state, setState, {
    undo, redo, clear,
    canUndo: past.current.length > 0,
    canRedo: future.current.length > 0,
  }];
}
