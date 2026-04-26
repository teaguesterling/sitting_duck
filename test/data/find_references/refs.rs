fn helper() -> i32 {
    42
}

fn process(data: &str) -> String {
    let result = transform(data);
    helper();
    result
}

fn transform(data: &str) -> String {
    data.to_string()
}

struct Service {
    db: String,
}

impl Service {
    fn new(db: &str) -> Self {
        Service { db: db.to_string() }
    }

    fn fetch(&self, query: &str) -> String {
        self.db.clone()
    }

    fn run(&self) -> String {
        let data = self.fetch("SELECT 1");
        process(&data)
    }
}

fn main() {
    let svc = Service::new("db");
    let result = svc.run();
    println!("{}", result);
}
