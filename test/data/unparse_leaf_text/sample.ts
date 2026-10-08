// line comment
/* block comment */
interface Greeting {
  name: string;
  count: number;
}

const g: Greeting = { name: "world", count: 42 };

function greet(input: Greeting): string {
  return `hello ${input.name} ${input.count}`;
}

greet(g);
