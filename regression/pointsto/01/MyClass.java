public final class MyClass {

  public static void change_second(MyClass param) {
    final MyClass local = param;
    final MyClass local2 = local.next;
    local2.next = new MyClass();
  }

  public static void change_nth(MyClass param) {
    MyClass victim = param;
    while (victim.value != 0)
      victim = victim.next;
    victim.next = new MyClass();
  }

  public static void swap_tails(MyClass x, MyClass y) {
    MyClass  temp = x.next;
    x.next = y.next;
    y.next = temp;
  }

  private int value;
  private MyClass next;
}
