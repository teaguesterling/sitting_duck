import os
import json

def helper():
    return 42

def process(data):
    result = transform(data)
    validated = validate(result)
    os.path.exists("/tmp")
    return validated

def transform(data):
    return json.loads(data)

def validate(data):
    return len(data) > 0

class Service:
    def __init__(self, db):
        self.db = db

    def fetch(self, query):
        result = self.db.execute(query)
        return result.fetchall()

    def process_all(self):
        data = self.fetch("SELECT 1")
        return transform(data)

def outer():
    def inner():
        helper()
    inner()
    process("test")

print("module level")
