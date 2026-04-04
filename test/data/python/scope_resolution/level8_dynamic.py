# Level 8: Dynamic dispatch / unresolvable cases
# These CANNOT be statically resolved — document the limitations

from typing import Protocol

class Runnable(Protocol):
    def run(self) -> str: ...

class TaskA:
    def run(self):
        return "A"

class TaskB:
    def run(self):
        return "B"

# Dynamic dispatch: type of items[i] is unknown at parse time
def run_all(items: list):
    results = []
    for item in items:
        # item.run() — can't resolve: item could be TaskA or TaskB
        results.append(item.run())
    return results

# Factory pattern: return type depends on runtime value
def create_task(kind):
    if kind == "a":
        return TaskA()
    return TaskB()

# Monkey patching: method added at runtime
def add_method(obj, name, func):
    setattr(obj, name, func)

# Higher-order: callback type unknown
def apply(func, value):
    # func(value) — can't resolve: func is a parameter
    return func(value)

# Dictionary dispatch
handlers = {
    "a": TaskA().run,
    "b": TaskB().run,
}

def dispatch(key):
    # handlers[key]() — can't resolve: key is dynamic
    return handlers[key]()
