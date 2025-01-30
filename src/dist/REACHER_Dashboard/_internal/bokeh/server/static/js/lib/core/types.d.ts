import type { Anchor } from "./enums";
export declare const GeneratorFunction: GeneratorFunctionConstructor;
export declare const AsyncGeneratorFunction: AsyncGeneratorFunctionConstructor;
export type uint8 = number;
export type uint16 = number;
export type uint32 = number;
export type ByteOrder = "little" | "big";
export type ID = string;
export type Color = string | [name: string, alpha: number] | uint32 | [R: uint8, G: uint8, B: uint8, A?: number];
export type ColorArray = Uint32Array;
export declare const ColorArray: Uint32ArrayConstructor;
export type RGBAArray = Uint8ClampedArray;
export declare const RGBAArray: Uint8ClampedArrayConstructor;
export type DataType = "uint8" | "int8" | "uint16" | "int16" | "uint32" | "int32" | "float32" | "float64";
export type NDDataType = "bool" | DataType | "object";
export type TypedArray = Uint8Array | Int8Array | Uint16Array | Int16Array | Uint32Array | Int32Array | Float32Array | Float64Array;
export type FloatArray = Float32Array | Float64Array;
export type FloatArrayConstructor = Float32ArrayConstructor | Float64ArrayConstructor;
export type IntArray = Int8Array | Int16Array | Int32Array;
export type IntArrayConstructor = Int8ArrayConstructor | Int16ArrayConstructor | Int32ArrayConstructor;
export declare function infer_type(a0: Float64Array, a1?: FloatArray): Float64ArrayConstructor;
export declare function infer_type(a0: FloatArray, a1: Float64Array): Float64ArrayConstructor;
export declare function infer_type(a0: Float32Array, a1?: Float32Array): Float32ArrayConstructor;
export declare function infer_type(a0: FloatArray, a1: FloatArray): FloatArrayConstructor;
export declare function infer_type(a0: Arrayable<number>, a1?: Arrayable<number>): FloatArrayConstructor;
export type ScreenArray = Float32Array;
export declare const ScreenArray: Float32ArrayConstructor;
export declare function to_screen(array: Iterable<number>): ScreenArray;
export type Arrayable<T = any> = {
    readonly length: number;
    [n: number]: T;
    [Symbol.iterator](): IterableIterator<T>;
    slice(start?: number, end?: number): Arrayable<T>;
    sort(compare_fn?: (a: T, b: T) => number): Arrayable<T>;
    indexOf(element: T, from_index?: number): number;
};
export type ArrayableNew = {
    new <T>(n: number): Arrayable<T>;
};
export type ArrayableOf<T> = T extends unknown ? Arrayable<T> : never;
export type PlainObject<T = unknown> = {
    [key: string]: T;
};
export type Dict<T> = PlainObject<T> | Map<string, T>;
export type Data<T = unknown> = Dict<Arrayable<T>>;
export type Attrs = PlainObject<unknown>;
export type Size = {
    width: number;
    height: number;
};
export type Box = {
    x: number;
    y: number;
    width: number;
    height: number;
    origin?: Anchor;
};
export type Rect = {
    x0: number;
    y0: number;
    x1: number;
    y1: number;
};
export type Extents = {
    left: number;
    top: number;
    right: number;
    bottom: number;
};
export type Interval = {
    start: number;
    end: number;
};
export { BitSet as Indices } from "./util/bitset";
export type { RaggedArray } from "./util/ragged_array";
//# sourceMappingURL=types.d.ts.map