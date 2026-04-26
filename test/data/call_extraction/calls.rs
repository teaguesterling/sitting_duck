fn helper() -> i32 {
    42
}

fn process(data: &str) -> String {
    let result = transform(data);
    let validated = validate(&result);
    println!("processing");
    validated
}

fn transform(data: &str) -> String {
    data.to_string()
}

fn validate(data: &str) -> String {
    data.trim().to_string()
}

struct Service {
    db: String,
}

impl Service {
    fn new(db: &str) -> Self {
        Service { db: db.to_string() }
    }

    fn fetch(&self, query: &str) -> Vec<String> {
        self.db.execute(query)
    }

    fn process_all(&self) -> Vec<String> {
        let data = self.fetch("SELECT 1");
        transform(&data[0])
    }
}

fn outer() {
    fn inner() {
        helper();
    }
    inner();
    process("test");
}

fn main() {
    let svc = Service::new("db");
    println!("done");
}
