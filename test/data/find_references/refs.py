x = 10

def process(x):
    y = x + 1
    return y

def transform(data):
    x = data * 2
    result = process(x)
    return result

class Service:
    def __init__(self, db):
        self.db = db

    def fetch(self, query):
        return self.db.execute(query)

    def run(self):
        data = self.fetch("SELECT 1")
        return process(data)

def main():
    svc = Service("postgres")
    result = svc.run()
    print(result)
