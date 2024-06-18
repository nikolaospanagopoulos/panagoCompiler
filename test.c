int printf(const char *s);
int main() {
  const char *abc = "nikos";
  printf(abc);

  goto abc;
abc:
}
