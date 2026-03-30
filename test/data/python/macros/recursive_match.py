# Functions with return statements at various depths
def shallow_return(x):
    return x

def return_in_if(x):
    if x > 0:
        return x
    return 0

def deeply_nested_return(x):
    if x > 0:
        for i in range(x):
            if i % 2 == 0:
                return i
    return -1

# Functions with execute() calls at various depths
def direct_execute(db):
    db.execute("SELECT 1")

def nested_execute(db):
    if db:
        try:
            db.execute("SELECT 1")
        except:
            pass

def no_execute(db):
    db.query("SELECT 1")

# Nested function scoping test
def outer_with_execute(db):
    def inner():
        db.execute("SELECT 1")
    inner()

def outer_no_execute(db):
    def inner():
        db.execute("SELECT 1")
    pass
