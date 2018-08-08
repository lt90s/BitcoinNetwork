import datetime

class SimpleCache:
    def __init__(self):
        self._cache = {}

    def __getitem__(self, key):
        if key not in self._cache:
            return None

        value, timeout = self._cache[key]
        if timeout > datetime.datetime.now():
            return value
        else:
            return None 

    def get(self, key):
        return self.__getitem__(key)     

    def set(self, key, value, timeout):
        assert(timeout > 0)
        now = datetime.datetime.now()
        self._cache[key] = (value, now + datetime.timedelta(seconds=timeout))