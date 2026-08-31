@Deprecated
public abstract class Base {
    private static final int MAX = 10;

    @Override
    public synchronized String process(int x) {
        return "";
    }

    @Deprecated
    private int legacy() {
        return 0;
    }

    public void plain() {}
}
