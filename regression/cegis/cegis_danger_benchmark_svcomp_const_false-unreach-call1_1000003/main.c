int main(void) {
  unsigned int x = 1;
  unsigned int y = 0;

  while (y < 1000003) {
    x = 0;
    y++;
  }

  __CPROVER_assert(x == 1, "A");

  return 0;
}
