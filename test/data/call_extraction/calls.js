function helper() {
    return 42;
}

function process(data) {
    const result = transform(data);
    console.log("processing");
    return result;
}

function transform(data) {
    return JSON.parse(data);
}

class Service {
    constructor(db) {
        this.db = db;
    }

    fetch(query) {
        const result = this.db.execute(query);
        return result.fetchAll();
    }

    processAll() {
        const data = this.fetch("SELECT 1");
        return transform(data);
    }
}

function outer() {
    function inner() {
        helper();
    }
    inner();
    process("test");
}

const svc = new Service("db");
console.log("module level");
