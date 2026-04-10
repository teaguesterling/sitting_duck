"""Sample application for tutorial demonstration."""
import os
import json
from pathlib import Path

# Configuration
MAX_RETRIES = 3
DEFAULT_TIMEOUT = 30

class Config:
    """Application configuration."""
    def __init__(self, path):
        self.path = path
        self.data = {}

    def load(self):
        with open(self.path) as f:
            self.data = json.load(f)
        return self

    def get(self, key, default=None):
        return self.data.get(key, default)


class DatabaseConnection:
    """Database wrapper with connection management."""
    def __init__(self, url, timeout=DEFAULT_TIMEOUT):
        self.url = url
        self.timeout = timeout
        self._conn = None

    def connect(self):
        self._conn = create_connection(self.url, timeout=self.timeout)
        return self

    def execute(self, query, params=None):
        if not self._conn:
            raise RuntimeError("Not connected")
        return self._conn.execute(query, params)

    def fetch_all(self, query, params=None):
        result = self.execute(query, params)
        return result.fetchall()

    def close(self):
        if self._conn:
            self._conn.close()
            self._conn = None


class UserService:
    """Service for managing users."""
    def __init__(self, db):
        self.db = db

    def get_user(self, user_id):
        rows = self.db.fetch_all(
            "SELECT * FROM users WHERE id = ?",
            [user_id]
        )
        if rows:
            return rows[0]
        return None

    def create_user(self, name, email):
        self.db.execute(
            "INSERT INTO users (name, email) VALUES (?, ?)",
            [name, email]
        )
        return self.get_user(email)

    def delete_user(self, user_id):
        user = self.get_user(user_id)
        if user:
            self.db.execute(
                "DELETE FROM users WHERE id = ?",
                [user_id]
            )
        return user

    def search_users(self, query):
        # WARNING: SQL injection vulnerability for demonstration
        results = self.db.fetch_all(
            f"SELECT * FROM users WHERE name LIKE '%{query}%'"
        )
        return results

    def bulk_import(self, users):
        for user in users:
            try:
                self.create_user(user["name"], user["email"])
            except Exception as e:
                print(f"Failed to import {user}: {e}")
                continue

    def export_users(self):
        return self.db.fetch_all("SELECT * FROM users")


def process_file(path):
    """Process a single file."""
    if not os.path.exists(path):
        return None
    with open(path) as f:
        data = json.load(f)
    return transform(data)


def transform(data):
    """Transform data dictionary."""
    result = {}
    for key, value in data.items():
        if isinstance(value, str):
            result[key] = value.strip()
        elif isinstance(value, list):
            result[key] = [v for v in value if v is not None]
        else:
            result[key] = value
    return result


def validate_email(email):
    """Simple email validation."""
    if "@" not in email:
        return False
    parts = email.split("@")
    if len(parts) != 2:
        return False
    return len(parts[1]) > 0


def retry(func, max_retries=MAX_RETRIES):
    """Retry a function with exponential backoff."""
    for attempt in range(max_retries):
        try:
            return func()
        except Exception as e:
            if attempt == max_retries - 1:
                raise
            wait = 2 ** attempt
            print(f"Retry {attempt + 1}/{max_retries} after {wait}s: {e}")


def main():
    """Application entry point."""
    config = Config("config.json").load()
    db = DatabaseConnection(config.get("db_url"))
    db.connect()

    try:
        service = UserService(db)
        users = service.export_users()
        print(f"Found {len(users)} users")

        for user in users:
            if validate_email(user["email"]):
                process_file(f"data/{user['id']}.json")
    finally:
        db.close()


if __name__ == "__main__":
    main()
