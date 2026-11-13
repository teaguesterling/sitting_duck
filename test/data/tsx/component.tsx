// A React component: JSX plus TypeScript types in one file.
// Read with language := 'typescript' this file is mostly ERROR nodes, which is
// why tsx is a separate language. Keep it small: it is a parse fixture, not a
// style example.
interface Props {
	title: string;
	count?: number;
}

export function Badge({ title, count }: Props) {
	const label = (count ?? 0) > 99 ? "99+" : String(count ?? 0);
	return (
		<span className="badge" title={title}>
			{label}
		</span>
	);
}

export const Empty = () => <span className="badge" />;

export function parseCount(raw: unknown): number {
	return (raw as number) ?? 0;
}
