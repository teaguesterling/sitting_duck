import os
import pickle
def dangerous_eval(user_input):
    result = eval(user_input)
    return result
def run_command(cmd):
    os.system(cmd)
def load_data(filename):
    with open(filename, "rb") as f:
        return pickle.load(f)
def safe_function():
    return 42
