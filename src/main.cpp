// common lib
#include <assert.h>
#include <stdio.h>

// GLFW for window create
#include <GLFW/glfw3.h>

// vulkan header file
#include <vulkan/vulkan.h>

int main()
{
	printf("hello, vulkan!\n");

	int ret = glfwInit();
	assert(ret);

	GLFWwindow* window = glfwCreateWindow(1024, 768, "My Renderer", 0, 0);
	assert(window);


	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	glfwDestroyWindow(window);

	return 0;
}