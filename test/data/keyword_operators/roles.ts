// #231 fixture: typeof serves BOTH roles in TypeScript off one declaration.
const value: unknown = 1;
type ValueType = typeof value;             // typeof: type-query position
const kind = typeof value;                 // typeof: value position
const widened = value as string;           // as: type assertion
const keys: keyof Window = "name";         // keyof: already TYPE_REFERENCE
