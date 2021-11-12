import os

this = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.abspath(os.path.join(this, "../.."))


def filepath(path):
    return os.path.join(project_root, path)
