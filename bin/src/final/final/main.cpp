#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/camera.h>
#include <learnopengl/Model.h>

#include "wood.h"
#include "skybox.h"
#include "plane.h"
#include "resource_manager.h"

#include <iostream>

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);
void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

const GLuint SCREEN_WIDTH = 800;

const GLuint SCREEN_HEIGHT = 600;

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCREEN_WIDTH / 2.0f;
float lastY = SCREEN_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

int main(int argc, char *argv[])
{
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

  GLFWwindow *window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "final", nullptr, nullptr);
  if (window == NULL)
  {
    std::cout << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetCursorPosCallback(window, mouse_callback);
  glfwSetScrollCallback(window, scroll_callback);

  // tell GLFW to capture our mouse
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  glfwSetKeyCallback(window, key_callback);
  // glad: load all OpenGL function pointers
  // ---------------------------------------
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
  {
    std::cout << "Failed to initialize GLAD" << std::endl;
    return -1;
  }
  // OpenGL configuration

  glEnable(GL_DEPTH_TEST);

  // laod frog

  Shader ourShader(FileSystem::getPath("src/final/final/model.vs").c_str(), FileSystem::getPath("src/final/final/model.fs").c_str());
  Model ourModel("./resources/frog/Rane Erba.obj");
  ourShader.setInt("shadowMap", 10);
  // load wood
  ResourceManager::LoadTexture("./resources/textures/wood.jpg", false, "wood");
  auto woodShader = ResourceManager::LoadShader(FileSystem::getPath("src/final/final/wood.vs").c_str(), FileSystem::getPath("src/final/final/wood.fs").c_str(), nullptr, "wood");
  auto wood = Wood(glm::vec3(0, 0, 0), glm::vec3(2, 0.8, 2), glm::vec3(1));

  // load skybox
  ResourceManager::LoadShader(FileSystem::getPath("src/final/final/skybox.vs").c_str(), FileSystem::getPath("src/final/final/skybox.fs").c_str(), nullptr, "skybox");
  std::vector<std::string>
      faces{
          "./resources/textures/sunny/sunny_lf.jpg",
          "./resources/textures/sunny/sunny_rt.jpg",
          "./resources/textures/sunny/sunny_up.jpg",
          "./resources/textures/sunny/sunny_dn.jpg",
          "./resources/textures/sunny/sunny_ft.jpg",
          "./resources/textures/sunny/sunny_bk.jpg"};
  Skybox skybox(glm::vec3(1), glm::vec3(1), glm::vec3(1), faces, "skybox_0");

  // load plane
  auto planeShader = ResourceManager::LoadShader(FileSystem::getPath("src/final/final/plane.vs").c_str(), FileSystem::getPath("src/final/final/plane.fs").c_str(), nullptr, "plane");
  planeShader.use();
  planeShader.setInt("texture1", 0);
  planeShader.setInt("texture2", 1);
  planeShader.setInt("mask", 2);
  planeShader.setInt("shadowMap", 3);
  ResourceManager::LoadTexture("./resources/textures/plane/grass.png", true, "grass");
  ResourceManager::LoadTexture("./resources/textures/plane/mountain.png", true, "mountain");
  ResourceManager::LoadTexture("./resources/textures/plane/mask.png", false, "mask");

  Plane plane(glm::vec3(-100, 0, -100), glm::vec3(0.3, 0.3, 0.3), glm::vec3(1), "./resources/textures/plane/height_1.jpg");

  auto shadowShader = ResourceManager::LoadShader(FileSystem::getPath("src/final/final/shadow_mapping.vs").c_str(), FileSystem::getPath("src/final/final/shadow_mapping.fs").c_str(), nullptr, "shadow_mapping");
  auto depthShader = ResourceManager::LoadShader(FileSystem::getPath("src/final/final/shadow_mapping_depth.vs").c_str(), FileSystem::getPath("src/final/final/shadow_mapping_depth.fs").c_str(), nullptr, "shadow_mapping_depth");

  const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
  unsigned int depthMapFBO;
  glGenFramebuffers(1, &depthMapFBO);
  // create depth texture
  unsigned int depthMap;
  glGenTextures(1, &depthMap);
  glBindTexture(GL_TEXTURE_2D, depthMap);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  float borderColor[] = {1.0, 1.0, 1.0, 1.0};
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
  // attach depth texture as FBO's depth buffer
  glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  glm::vec3 lightPos(5.0f, 15.0f, 5.0f);

  while (!glfwWindowShouldClose(window))
  {
    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
    processInput(window);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //lightPos.x = 15 + 10 * sin(glfwGetTime());

    // 1. render depth of scene to texture (from light's perspective)
    // --------------------------------------------------------------
    glm::mat4 lightProjection, lightView;
    glm::mat4 lightSpaceMatrix;
    float near_plane = 1.0f, far_plane = 50.5f;
    lightProjection = glm::ortho(-50.0f, 50.0f, -50.0f, 50.0f, near_plane, far_plane);
    lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
    lightSpaceMatrix = lightProjection * lightView;
    // render scene from light's point of view
    depthShader.use();
    depthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);

    wood.Draw(&depthShader);
    plane.Draw(&depthShader);
    ourModel.Draw(depthShader);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // reset viewport
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 2. render scene as normal using the generated depth/shadow map
    // --------------------------------------------------------------
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    shadowShader.use();
    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 200.0f);
    glm::mat4 view = camera.GetViewMatrix();
    shadowShader.setMat4("projection", projection);
    shadowShader.setMat4("view", view);
    // set light uniforms
    shadowShader.setVec3("viewPos", camera.Position);
    shadowShader.setVec3("lightPos", lightPos);
    shadowShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    wood.Draw(&shadowShader);
    //plane.Draw(&shadowShader);

    auto planeShader = ResourceManager::GetShader("plane");
    planeShader.use();
    glActiveTexture(GL_TEXTURE0); // 在绑定纹理之前先激活纹理单元
    glBindTexture(GL_TEXTURE_2D, ResourceManager::GetTexture("mountain").ID);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, ResourceManager::GetTexture("grass").ID);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, ResourceManager::GetTexture("mask").ID);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    planeShader.setMat4("projection", projection);
    planeShader.setMat4("view", view);
    planeShader.setVec3("viewPos", camera.Position);
    planeShader.setVec3("lightPos", lightPos);
    planeShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
    plane.Draw(&planeShader);

    // draw frog
    ourShader.use();
    ourShader.setVec3("light.position", lightPos);
    ourShader.setVec3("lightPos", lightPos);
    ourShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
    ourShader.setVec3("viewPos", camera.Position);
    ourShader.setVec3("light.ambient", 0.3f, 0.3f, 0.3f);
    ourShader.setVec3("light.diffuse", 1.0f, 1.0f, 1.0f);
    ourShader.setVec3("light.specular", 1.0f, 1.0f, 1.0f);
    ourShader.setFloat("shininess", 64.0f);
    ourShader.setMat4("projection", projection);
    ourShader.setMat4("view", view);
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(5.0f, 0.2f, -3.0f)); // translate it down so it's at the center of the scene
    model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.2f));      // it's a bit too big for our scene, so scale it down
    ourShader.setMat4("model", model);
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    ourModel.Draw(ourShader);

    auto skyboxShader = ResourceManager::GetShader("skybox");
    skyboxShader.use();
    view = glm::mat4(glm::mat3(camera.GetViewMatrix())); // remove translation from the view matrix
    skyboxShader.setMat4("view", view);
    skyboxShader.setMat4("projection", projection);
    skybox.Draw(&skyboxShader);

    glfwMakeContextCurrent(window);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // Delete all resources as loaded using the resource manager
  ResourceManager::Clear();

  glfwTerminate();
  return 0;
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
  // When a user presses the escape key, we set the WindowShouldClose property to true, closing the application
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GL_TRUE);
  if (key >= 0 && key < 1024)
  {
    if (action == GLFW_PRESS)
    {
    }

    else if (action == GLFW_RELEASE)
    {
    }
  }
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
  float dt = deltaTime * 10;
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    camera.ProcessKeyboard(FORWARD, dt);
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    camera.ProcessKeyboard(BACKWARD, dt);
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    camera.ProcessKeyboard(LEFT, dt);
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    camera.ProcessKeyboard(RIGHT, dt);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
  // make sure the viewport matches the new window dimensions; note that width and
  // height will be significantly larger than specified on retina displays.
  glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
  if (firstMouse)
  {
    lastX = xpos;
    lastY = ypos;
    firstMouse = false;
  }

  float xoffset = xpos - lastX;
  float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

  lastX = xpos;
  lastY = ypos;

  camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
  camera.ProcessMouseScroll(yoffset);
}