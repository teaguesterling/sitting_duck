const config = { timeout: 30 };

function helper(x) {
    return x * 2;
}

function process(data) {
    const x = helper(data);
    console.log(x);
    return x;
}

class Service {
    constructor(db) {
        this.db = db;
    }

    fetch(query) {
        return this.db.execute(query);
    }

    run() {
        const data = this.fetch("SELECT 1");
        return process(data);
    }
}

const svc = new Service("postgres");
const result = svc.run();
console.log(result);
