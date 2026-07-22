class Widget {
    static int Helper(int x) {
        return x + 1;
    }

    int Render(string s) {
        var t = s.Trim();
        return Helper(t.Length);
    }
}
