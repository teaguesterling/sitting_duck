# Level 5: Method resolution through instance type
# obj.method() requires resolving obj's type, then finding method on that type

class Validator:
    def validate(self, data):
        return len(data) > 0

    def validate_email(self, email):
        return "@" in email


class Processor:
    def __init__(self):
        self.validator = Validator()

    def process(self, data):
        # self.validator.validate(data) requires:
        # 1. self.validator → Processor.__init__'s assignment
        # 2. Validator() → Validator class
        # 3. .validate → Validator.validate method
        if self.validator.validate(data):
            return self.transform(data)
        return None

    def transform(self, data):
        return data.strip()


# Also: local variable type resolution
def run():
    proc = Processor()
    # proc.process("hello") requires:
    # 1. proc → local assignment
    # 2. Processor() → Processor class
    # 3. .process → Processor.process method
    result = proc.process("hello")
    return result
