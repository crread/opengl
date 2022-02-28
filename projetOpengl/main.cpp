#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <IntSafe.h>

extern "C"
{
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

#if defined(_WIN32) && defined(_MSC_VER)
#pragma comment(lib, "glfw3dll.lib")
#pragma comment(lib, "glew32s.lib")			// glew32.lib si pas GLEW_STATIC
#pragma comment(lib, "opengl32.lib")
#elif defined(__APPLE__)
#elif defined(__linux__)
#endif

#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "DragonData.h"

// format des vertex du dragon XY-NXNYNZ-UV
struct Vertex {
	//float x, y, z; 
	//float nx, ny, nz; 
	//float u, v;
	float position[3];
	float normal[3];
	float uv[2];
};

// ou bien std::uint32_t 
GLuint objectVAO;	// la structure d'attributs stockee en VRAM
GLuint objectVBO;	// les vertices de l'objet stockees en VRAM
GLuint objectIBO;	// les indices de l'objet stockees en VRAM
GLuint texID;

// vous pouvez ajouter ce repertoire directement dans les proprietes du projet
#include "GLShader.h"

GLShader lightShader;
GLShader copyShader;

// identifiant du Framebuffer Object
GLuint FBO;
GLuint ColorBufferFBO; // stocke les couleurs du rendu hors ecran
GLuint DepthBufferFBO; // stocke les Z du rendu hors ecran

uint16_t objectIndice;
float objectvertice;

void Initialize()
{
	GLenum error = glewInit();
	if (error != GLEW_OK) {
		std::cout << "erreur d'initialisation de GLEW!"
			<< std::endl;
	}

	std::cout << "Version : " << glGetString(GL_VERSION) << std::endl;
	std::cout << "Vendor : " << glGetString(GL_VENDOR) << std::endl;
	std::cout << "Renderer : " << glGetString(GL_RENDERER) << std::endl;

	lightShader.LoadVertexShader("basicLight.vs");
	lightShader.LoadFragmentShader("basicLight.fs");
	lightShader.Create();

	copyShader.LoadVertexShader("postprocess.vs");
	copyShader.LoadFragmentShader("postprocess.fs");
	copyShader.Create();

	// --- load tinyOBJ ---

	std::string modelPath = "../models/roadBike/roadBike.obj";

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warn;
	std::string err;

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str());

	if (!warn.empty()) {
		std::cout << warn << std::endl;
	}

	if (!err.empty()) {
		std::cerr << err << std::endl;
	}

	if (!ret) {
		exit(1);
	}

	// Loop over shapes
	for (size_t s = 0; s < shapes.size(); s++) {
		// Loop over faces(polygon)
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
			int fv = shapes[s].mesh.num_face_vertices[f];

			// Loop over vertices in the face.
			for (size_t v = 0; v < fv; v++) {
				// access to vertex
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
				tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
				tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
				tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
				tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
				tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
				tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];
				tinyobj::real_t tx = attrib.texcoords[2 * idx.texcoord_index + 0];
				tinyobj::real_t ty = attrib.texcoords[2 * idx.texcoord_index + 1];
			}
			index_offset += fv;

			// per-face material
			shapes[s].mesh.material_ids[f];
		}
	}

	// --- FBO ----

	glGenFramebuffers(1, &FBO);
	// attache une texture comme color buffer
	glGenTextures(1, &ColorBufferFBO);
	glBindTexture(GL_TEXTURE_2D, ColorBufferFBO);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1280, 720, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	// attache une texture comme depth buffer
	glGenTextures(1, &DepthBufferFBO);
	glBindTexture(GL_TEXTURE_2D, DepthBufferFBO);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 1280, 720, 0,
		GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);

	// bind FBO + attache la texture que l'on vient de creer
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D, ColorBufferFBO, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
		GL_TEXTURE_2D, DepthBufferFBO, 0);
	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// ---

	glGenTextures(1, &texID);
	glBindTexture(GL_TEXTURE_2D, texID);
	int texWidth, texHeight, c;
	uint8_t* pixels =
		stbi_load("batman_logo.png", &texWidth, &texHeight, &c, STBI_rgb_alpha);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texWidth, texHeight, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glGenerateMipmap(GL_TEXTURE_2D);
	stbi_image_free(pixels);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	
	glGenVertexArrays(1, &objectVAO);
	glBindVertexArray(objectVAO);
	// ATTENTION : avec glBindVertexArray TOUS les appels suivants sont 
	// enregistres dans le VAO 
	// ca inclus glBindBuffer(GL_ARRAY_BUFFER/GL_ELEMENT_ARRAY_BUFFER..)
	// ainsi que tous les appels glVertexAttribPointer(), glEnable/disableVertexAttribArray
	glGenBuffers(1, &objectVBO);
	glBindBuffer(GL_ARRAY_BUFFER, objectVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(DragonVertices), DragonVertices, GL_STATIC_DRAW);

	glGenBuffers(1, &objectIBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, objectIBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(DragonIndices), DragonIndices, GL_STATIC_DRAW);

	constexpr int STRIDE = sizeof(Vertex);
	// nos positions sont XYZ (3 floats)
	const int POSITION =
	glGetAttribLocation(lightShader.GetProgram(), "a_position");
	glEnableVertexAttribArray(POSITION);
	glVertexAttribPointer(POSITION, 3, GL_FLOAT, false, STRIDE, (void*)offsetof(Vertex, position));
	// nos normales sont en 3D aussi (3 floats)
	const int NORMAL =
	glGetAttribLocation(lightShader.GetProgram(), "a_normal");
	glEnableVertexAttribArray(NORMAL);
	glVertexAttribPointer(NORMAL, 3, GL_FLOAT, false, STRIDE, (void*)offsetof(Vertex, normal));

	// NECESSAIRE POUR LES TEXTURES ---
	const int UV =
	glGetAttribLocation(lightShader.GetProgram(), "a_texcoords");
	glEnableVertexAttribArray(UV);
	glVertexAttribPointer(UV, 2, GL_FLOAT, false, STRIDE, (void*)offsetof(Vertex, uv));

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Shutdown()
{
	glDeleteVertexArrays(1, &objectVAO);
	glDeleteBuffers(1, &objectIBO);
	glDeleteBuffers(1, &objectVBO);

	glDeleteTextures(1, &texID);

	glDeleteTextures(1, &ColorBufferFBO);
	glDeleteTextures(1, &DepthBufferFBO);
	glDeleteFramebuffers(1, &FBO);


	lightShader.Destroy();

	copyShader.Destroy();
}

void Display(GLFWwindow* window)
{
	int width, height;

	// rendu hors ecran
	int offscreenWidth = 1280, offscreenHeight = 720;

	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	glViewport(0, 0, offscreenWidth, offscreenHeight);

	// attention le depth mask doit etre active avant d'ecrire (ou clear) le depth buffer
	glDepthMask(GL_TRUE);

	glClearColor(1.f, 1.f, 0.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// active le tri des faces
	glEnable(GL_DEPTH_TEST);
	// on peut egalement modifier la fonction de test
	//glDepthFunc(GL_LEQUAL); // par ex: en inferieur ou egal
	// active la suppression des faces arrieres
	glEnable(GL_CULL_FACE);

	glUseProgram(lightShader.GetProgram());

	// ----
	glActiveTexture(GL_TEXTURE0); // on la bind sur la texture unit #0
	glBindTexture(GL_TEXTURE_2D, texID);

	const int sampler =
		glGetUniformLocation(lightShader.GetProgram(), "u_sampler");
	glUniform1i(sampler, 0); // Le '0' correspond au '0' dans glActiveTexture()
	// ----

	// affecte le temps ecoules depuis le debut du programme à "u_time"
	const int timeLocation =
		glGetUniformLocation(lightShader.GetProgram(), "u_time");
	float time = static_cast<float>(glfwGetTime());
	glUniform1f(timeLocation, time);

	//
	// MATRICE DE SCALE
	//
	const float scale[] = {
		10.f, 0.f, 0.f, 0.f, // 1ere colonne
		0.f, 10.f, 0.f, 0.f,
		0.f, 0.f, 10.f, 0.f,
		0.f, 0.f, 0.f, 1.f // 4eme colonne
	};
	const GLint matScaleLocation = glGetUniformLocation(
		lightShader.GetProgram(),
		"u_scale"
	);
	glUniformMatrix4fv(matScaleLocation, 1, false, scale);

	//
	// MATRICE DE ROTATION
	//
	const float rot[] = {
	1.f, 0.f, 0.f, 0.f, // 1ere colonne
	0.f, 1.f, 0.f, 0.f,
	0.f, 0.f, 1.f, 0.f,
	0.f, 0.f, 0.f, 1.f // 4eme colonne
	};
	const float rotY[] = {
		cosf(time), 0.f, -sinf(time), 0.f, // 1ere colonne
		0.f,		1.f,	     0.f, 0.f,
		sinf(time),	0.f,  cosf(time), 0.f,
		0.f,		0.f,	 	 0.f, 1.f // 4eme colonne
	};
	const GLint matRotLocation = glGetUniformLocation(
		lightShader.GetProgram(),
		"u_rotation"
	);
	glUniformMatrix4fv(matRotLocation, 1, false, rotY);

	//
	// MATRICE DE TRANSLATION
	//
	const float translation[] = {
		1.f, 0.f, 0.f, 0.f, // 1ere colonne
		0.f, 1.f, 0.f, 0.f,
		0.f, 0.f, 1.f, 0.f,
		0.f, 0.f, -200.f, 1.f // 4eme colonne
	};
	const GLint matTranslationLocation = glGetUniformLocation(
		lightShader.GetProgram(),
		"u_translation"
	);
	glUniformMatrix4fv(matTranslationLocation, 1, false, translation);

	//
	// MATRICE DE PROJECTION
	//
	const float aspectRatio = float(offscreenWidth) / float(offscreenHeight);
	constexpr float nearZ = 0.01f;
	constexpr float farZ = 1000.f;
	constexpr float fov = 45.f;
	constexpr float fov_rad = fov * 3.141592654f / 180.f;
	const float f = 1.f / tanf(fov_rad / 2.f);
	const float projectionPerspective[] = {
		f / aspectRatio, 0.f, 0.f, 0.f, // 1ere colonne
		0.f, f, 0.f, 0.f,
		0.f, 0.f, -(farZ + nearZ) / (farZ - nearZ), -1.f,
		0.f, 0.f, -(2.f * farZ * nearZ) / (farZ - nearZ), 0.f // 4eme colonne
	};

	const GLint matProjectionLocation = glGetUniformLocation(
		lightShader.GetProgram(),
		"u_projection"
	);
	glUniformMatrix4fv(matProjectionLocation, 1, false, projectionPerspective);

	glBindVertexArray(objectVAO);
	// GL_UNSIGNED_SHORT car le tableau DragonIndices est compose de unsigned short (uint16_t)
	// le dernier 0 indique que les indices proviennent d'un ELEMENT_ARRAY_BUFFER
	glDrawElements(GL_TRIANGLES, _countof(DragonIndices), GL_UNSIGNED_SHORT, 0);

	// Etape finale
	// retour vers le back buffer
	glfwGetWindowSize(window, &width, &height);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	// Defini le viewport en pleine fenetre
	glViewport(0, 0, width, height);

	// ex1.1 
	//glBindFramebuffer(GL_READ_FRAMEBUFFER, FBO);
	////glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // redondant ici
	//glBlitFramebuffer(0, 0, 1280, 720,	// min;max de la source 
	//	0, 0, width, height, // min;max destination
	//	GL_COLOR_BUFFER_BIT, GL_LINEAR);

	// TODO: commenter les lignes precedentes + ex1.2 et 2.1

	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	GLuint postprocess_program = copyShader.GetProgram();
	glUseProgram(postprocess_program);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ColorBufferFBO);
	GLint copyLoc = glGetUniformLocation(postprocess_program, "u_Texture");
	glUniform1i(copyLoc, 0);

	glBindVertexArray(0); // desactive le(s) VAO
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	const float quad[] = { -1.f, +1.f, -1.f, -1.f, +1.f, +1.f, +1.f, -1.f };
	// les texcoords sont calculees sur la base de la position (xy * 0.5 + 0.5)
	glVertexAttribPointer(0, 2, GL_FLOAT, false, sizeof(float) * 2, quad);
	glEnableVertexAttribArray(0);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); // {0,1,2}{1,2,3}
}

static void error_callback(int error, const char* description)
{
	std::cout << "Error GFLW " << error << " : " << description << std::endl;
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
}

// voir https://www.glfw.org/documentation.html
// et https://www.glfw.org/docs/latest/quick.html
int main(void)
{
	GLFWwindow* window;

	glfwSetErrorCallback(error_callback);

	/* Initialize the library */
	if (!glfwInit())
		return -1;

	/* Create a windowed mode window and its OpenGL context */
	window = glfwCreateWindow(640, 480, "projet openGL", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		return -1;
	}

	/* Make the window's context current */
	glfwMakeContextCurrent(window);

	// defini la fonction de rappel utilisee lors de l'appui d'une touche
	glfwSetKeyCallback(window, key_callback);

	// toutes nos initialisations vont ici
	Initialize();

	/* Loop until the user closes the window */
	while (!glfwWindowShouldClose(window))
	{
		/* Render here */
		Display(window);

		/* Swap front and back buffers */
		glfwSwapBuffers(window);

		/* Poll for and process events */
		glfwPollEvents();
	}

	// ne pas oublier de liberer la memoire etc...
	Shutdown();

	glfwTerminate();
	return 0;
}