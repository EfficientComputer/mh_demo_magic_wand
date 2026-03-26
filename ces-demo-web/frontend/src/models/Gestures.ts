import RingImg from '../assets/Ring.png';
import SlopeImg from '../assets/Slope.png';
import WingImg from '../assets/Wing.png';
import UnknownImg from '../assets/Unknown.png';

export const Gestures = {
  WING: 0,
  RING: 1,
  SLOPE: 2,
  UNKNOWN: 3
} as const;

export type Gestures = typeof Gestures[keyof typeof Gestures];

export const GestureChars = ['W', 'O', '∠', '?'];

export const GestureImages = [WingImg, RingImg, SlopeImg, UnknownImg];

export const getGestureImage = (index: number): string => {
  return GestureImages[index] ?? UnknownImg;
};