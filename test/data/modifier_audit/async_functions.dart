Future<String> fetchData(String url) async {
  return await http.get(url);
}

String syncHelper() {
  return 'hello';
}

Stream<int> generateNumbers() async* {
  for (var i = 0; i < 10; i++) {
    yield i;
  }
}

class Service {
  static void staticHelper() {
    print('static');
  }

  Future<void> asyncMethod() async {
    await Future.delayed(Duration(seconds: 1));
  }
}
