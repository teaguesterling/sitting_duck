interface Shape {
    name: string;
    area(scale: number): number;
}

function build(label: string, size: number): Shape {
    return { name: label, area: (s: number) => size * s };
}
