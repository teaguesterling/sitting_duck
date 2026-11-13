// The .ts sibling of component.tsx. It holds no JSX, so the typescript grammar
// parses it cleanly; it exists so one directory carries both extensions and a
// mixed glob has something to detect per file.
export interface Props {
	title: string;
	count?: number;
}

export function parseCount(raw: unknown): number {
	return (raw as number) ?? 0;
}
