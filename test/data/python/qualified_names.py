MY_CONST = 42


class User:
    def __init__(self, name):
        self.name = name

    def validate(self):
        return len(self.name) > 0


class Account:
    def __init__(self, owner):
        self.owner = owner

    class Settings:
        def __init__(self, theme="dark"):
            self.theme = theme


def top_level_function():
    def nested_helper():
        return True

    return nested_helper()
