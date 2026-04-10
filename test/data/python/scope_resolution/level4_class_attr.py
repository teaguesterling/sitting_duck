# Level 4: Class attribute / self resolution
# self.x should resolve through __init__ assignment

class Config:
    def __init__(self, path):
        self.path = path
        self.data = {}

    def load(self):
        # self.path should resolve to __init__'s self.path assignment
        with open(self.path) as f:
            self.data = f.read()
        return self.data

    def get(self, key):
        # self.data should resolve to __init__'s self.data assignment
        return self.data.get(key)


class Database:
    def __init__(self, url):
        self.url = url
        self._conn = None

    def connect(self):
        self._conn = create_connection(self.url)

    def execute(self, query):
        # self._conn should resolve to either __init__ or connect's assignment
        return self._conn.execute(query)
