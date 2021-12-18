def pytest_configure(config):
    config.addinivalue_line(
        "markers", "images_only: mark test to run only on image"
    )
