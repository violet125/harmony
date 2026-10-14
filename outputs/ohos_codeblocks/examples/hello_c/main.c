#include <stdio.h>

int main(void)
{
    int count = 0;
    printf("Hello from ohos_codeblocks!\n");
    printf("Enter a number from 1 to 10: ");
    fflush(stdout);
    if (scanf("%d", &count) != 1 || count < 1 || count > 10) {
        printf("Please enter an integer between 1 and 10.\n");
        return 1;
    }
    for (int i = 1; i <= count; ++i) {
        printf("%d squared = %d\n", i, i * i);
    }
    return 0;
}
