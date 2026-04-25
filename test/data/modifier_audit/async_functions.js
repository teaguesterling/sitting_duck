async function fetchData(url) {
  return await fetch(url);
}

function syncHelper() {
  return 42;
}

class Service {
  async start() {}
  stop() {}
  static create() {}
  static async initialize() {}
}
