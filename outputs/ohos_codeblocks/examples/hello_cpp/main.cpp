#include <iostream>
#include <vector>

int main()
{
    const std::vector<int> numbers{1, 2, 3, 4, 5};
    std::cout << "C++17 native build from ohos_codeblocks\n";
    for (const auto number : numbers) {
        std::cout << number << " squared = " << number * number << '\n';
    }
    return 0;
}
