func syncHelper() -> Int {
    return 42
}

func fetchData(url: String) async -> String {
    return await URLSession.shared.data(from: url)
}

static func staticHelper() -> Void {
    print("static")
}

mutating func update() {
    self.count += 1
}

public func publicHelper() -> Bool {
    return true
}
