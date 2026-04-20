async function fetchData(url: string): Promise<string> {
  return await fetch(url).then(r => r.text());
}

function syncHelper(): number {
  return 42;
}

export async function exportedAsync(): Promise<void> {
  await Promise.resolve();
}

class MyService {
  async start(): Promise<void> {}
  stop(): void {}
  static async create(): Promise<MyService> { return new MyService(); }
}
