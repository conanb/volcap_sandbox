#include "Visualiser.h"
#include <GL/glew.h>

Visualiser* Visualiser::sm_singleton = nullptr;

Visualiser::Visualiser(unsigned int a_uiMaxLines, unsigned int a_uiMaxTris)
	: m_maxLines(a_uiMaxLines),
	m_lineCount(0),
	m_lines(new VisualiserLine[a_uiMaxLines]),
	m_maxTris(a_uiMaxTris),
	m_triCount(0),
	m_tris(new VisualiserTri[a_uiMaxTris]),
	m_alphaTriCount(0),
	m_alphaTris(new VisualiserTri[a_uiMaxTris])
{
	glewInit();

	// create shaders
	const char* vsSource = "#version 330\n \
					 in vec4 Position; \
					 in vec4 Colour; \
					 out vec4 vColour; \
					 uniform mat4 View; \
					 uniform mat4 Projection; \
					 void main() \
					 { \
						 vColour = Colour; \
						 gl_Position = Projection * View * Position; \
					 }";

	const char* fsSource = "#version 330\n \
					 in vec4 vColour; \
					 out vec4 outColour; \
					 void main()	\
					 { \
						 outColour = vColour; \
					 }";

	m_vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(m_vertexShader, 1, (const char**)&vsSource, 0);
	glCompileShader(m_vertexShader);

	m_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(m_fragmentShader, 1, (const char**)&fsSource, 0);
	glCompileShader(m_fragmentShader);

	m_shaderProgram = glCreateProgram();
	glAttachShader(m_shaderProgram, m_vertexShader);
	glAttachShader(m_shaderProgram, m_fragmentShader);
	glBindAttribLocation(m_shaderProgram, 0, "Position");
	glBindAttribLocation(m_shaderProgram, 1, "Colour");
	glBindFragDataLocation(m_shaderProgram, 0, "outColour");
	glLinkProgram(m_shaderProgram);
	
	// create VBOs
	glGenBuffers( 1, &m_lineVBO );
	glBindBuffer(GL_ARRAY_BUFFER, m_lineVBO);
	glBufferData(GL_ARRAY_BUFFER, m_maxLines * sizeof(VisualiserLine), m_lines, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glGenBuffers( 1, &m_triVBO );
	glBindBuffer(GL_ARRAY_BUFFER, m_triVBO);
	glBufferData(GL_ARRAY_BUFFER, m_maxTris * sizeof(VisualiserTri), m_tris, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glGenBuffers( 1, &m_alphaTriVBO );
	glBindBuffer(GL_ARRAY_BUFFER, m_alphaTriVBO);
	glBufferData(GL_ARRAY_BUFFER, m_maxTris * sizeof(VisualiserTri), m_alphaTris, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	unsigned int uiIndexCount = m_maxTris * 3;
	unsigned int* auiIndices = new unsigned int[uiIndexCount];
	for ( unsigned int i = 0 ; i < uiIndexCount ; ++i )
		auiIndices[i] = i;
	glGenBuffers( 1, &m_triIBO );
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_triIBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, uiIndexCount * sizeof(unsigned int), auiIndices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glGenBuffers( 1, &m_alphaTriIBO );
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_alphaTriIBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, uiIndexCount * sizeof(unsigned int), auiIndices, GL_STATIC_DRAW);
	delete[] auiIndices;

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	uiIndexCount = m_maxLines * 2;
	auiIndices = new unsigned int[uiIndexCount];
	for ( unsigned int i = 0 ; i < uiIndexCount ; ++i )
		auiIndices[i] = i;
	glGenBuffers( 1, &m_lineIBO );
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_lineIBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, uiIndexCount * sizeof(unsigned int), auiIndices, GL_STATIC_DRAW);
	delete[] auiIndices;

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glGenVertexArrays(1, &m_lineVAO);
	glBindVertexArray(m_lineVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_lineVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_lineIBO);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(VisualiserVertex), 0);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_TRUE, sizeof(VisualiserVertex), ((char*)0) + 16);

	glGenVertexArrays(1, &m_triVAO);
	glBindVertexArray(m_triVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_triVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_triIBO);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(VisualiserVertex), 0);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_TRUE, sizeof(VisualiserVertex), ((char*)0) + 16);

	glGenVertexArrays(1, &m_alphaTriVAO);
	glBindVertexArray(m_alphaTriVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_alphaTriVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_alphaTriIBO);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(VisualiserVertex), 0);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_TRUE, sizeof(VisualiserVertex), ((char*)0) + 16);

	glBindVertexArray(0);
}

Visualiser::~Visualiser()
{
	delete[] m_lines;
	delete[] m_tris;
	delete[] m_alphaTris;
	glDeleteBuffers( 1, &m_lineVBO );
	glDeleteBuffers( 1, &m_lineIBO );
	glDeleteBuffers( 1, &m_triVBO );
	glDeleteBuffers( 1, &m_triIBO );
	glDeleteBuffers( 1, &m_alphaTriVBO );
	glDeleteBuffers( 1, &m_alphaTriIBO );
	glDeleteVertexArrays( 1, &m_lineVAO );
	glDeleteVertexArrays( 1, &m_triVAO );
	glDeleteVertexArrays( 1, &m_alphaTriVAO );
	glDeleteShader(m_vertexShader);
	glDeleteShader(m_fragmentShader);
	glDeleteProgram(m_shaderProgram);
}

Visualiser* Visualiser::create(unsigned int a_maxLines /* = 16384 */, unsigned int a_maxTris /* = 16384 */)
{
	if (sm_singleton == nullptr)
		sm_singleton = new Visualiser(a_maxLines,a_maxTris);
	return sm_singleton;
}

void Visualiser::destroy()
{
	delete sm_singleton;
	sm_singleton = nullptr;
}

void Visualiser::clear()
{
	m_lineCount = 0;
	m_triCount = 0;
	m_alphaTriCount = 0;
}

// Adds 3 unit-length lines (red,green,blue) representing the 3 axis of a transform, 
// at the transform's translation. Optional scale available.
void Visualiser::addTransform(const Eigen::Matrix4f& a_transform, float a_scale /* = 1.0f */)
{
	auto translate = a_transform.col(3);
	auto xAxis = translate + a_transform.col(0) * a_scale;
	auto yAxis = translate + a_transform.col(1) * a_scale;
	auto zAxis = translate + a_transform.col(2) * a_scale;

	Eigen::Vector4f red(1,0,0,1);
	Eigen::Vector4f green(0,1,0,1);
	Eigen::Vector4f blue(0,0,1,1);

	addLine(translate, xAxis, red, red);
	addLine(translate, yAxis, green, green);
	addLine(translate, zAxis, blue, blue);
}

void Visualiser::addAABB(const Eigen::Vector4f& a_center, 
	const Eigen::Vector4f& a_extents, 
	const Eigen::Vector4f& a_colour, 
	const Eigen::Matrix4f* a_transform /* = nullptr */)
{
	Eigen::Vector4f verts[8];
	Eigen::Vector4f x(a_extents[0], 0, 0, 0);
	Eigen::Vector4f y(0, a_extents[1], 0, 0);
	Eigen::Vector4f z(0, 0, a_extents[2], 0);

	if (a_transform != nullptr)
	{
		x = *a_transform * x;
		y = *a_transform * y;
		z = *a_transform * z;
	}

	// top verts
	verts[0] = a_center - x - z - y;
	verts[1] = a_center - x + z - y;
	verts[2] = a_center + x + z - y;
	verts[3] = a_center + x - z - y;

	// bottom verts
	verts[4] = a_center - x - z + y;
	verts[5] = a_center - x + z + y;
	verts[6] = a_center + x + z + y;
	verts[7] = a_center + x - z + y;

	addLine(verts[0], verts[1], a_colour, a_colour);
	addLine(verts[1], verts[2], a_colour, a_colour);
	addLine(verts[2], verts[3], a_colour, a_colour);
	addLine(verts[3], verts[0], a_colour, a_colour);

	addLine(verts[4], verts[5], a_colour, a_colour);
	addLine(verts[5], verts[6], a_colour, a_colour);
	addLine(verts[6], verts[7], a_colour, a_colour);
	addLine(verts[7], verts[4], a_colour, a_colour);

	addLine(verts[0], verts[4], a_colour, a_colour);
	addLine(verts[1], verts[5], a_colour, a_colour);
	addLine(verts[2], verts[6], a_colour, a_colour);
	addLine(verts[3], verts[7], a_colour, a_colour);
}

void Visualiser::addAABBFilled(const Eigen::Vector4f& a_center, 
	const Eigen::Vector4f& a_extents, 
	const Eigen::Vector4f& a_fillColour, 
	const Eigen::Matrix4f* a_transform /* = nullptr */)
{
	Eigen::Vector4f verts[8];
	Eigen::Vector4f x(a_extents[0], 0, 0, 0);
	Eigen::Vector4f y(0, a_extents[1], 0, 0);
	Eigen::Vector4f z(0, 0, a_extents[2], 0);

	if (a_transform != nullptr)
	{
		x = *a_transform * x;
		y = *a_transform * y;
		z = *a_transform * z;
	}

	// top verts
	verts[0] = a_center - x + z + y;
	verts[1] = a_center - x - z + y;
	verts[2] = a_center + x - z + y;
	verts[3] = a_center + x + z + y;

	// bottom verts
	verts[4] = a_center - x + z - y;
	verts[5] = a_center - x - z - y;
	verts[6] = a_center + x - z - y;
	verts[7] = a_center + x + z - y;

	Eigen::Vector4f white(1,1,1,1);

	addLine(verts[0], verts[1], white, white);
	addLine(verts[1], verts[2], white, white);
	addLine(verts[2], verts[3], white, white);
	addLine(verts[3], verts[0], white, white);

	addLine(verts[4], verts[5], white, white);
	addLine(verts[5], verts[6], white, white);
	addLine(verts[6], verts[7], white, white);
	addLine(verts[7], verts[4], white, white);

	addLine(verts[0], verts[4], white, white);
	addLine(verts[1], verts[5], white, white);
	addLine(verts[2], verts[6], white, white);
	addLine(verts[3], verts[7], white, white);

	// top
	addTri(verts[2], verts[1], verts[0], a_fillColour);
	addTri(verts[3], verts[2], verts[0], a_fillColour);

	// bottom
	addTri(verts[5], verts[6], verts[4], a_fillColour);
	addTri(verts[6], verts[7], verts[4], a_fillColour);

	// front
	addTri(verts[4], verts[3], verts[0], a_fillColour);
	addTri(verts[7], verts[3], verts[4], a_fillColour);

	// back
	addTri(verts[1], verts[2], verts[5], a_fillColour);
	addTri(verts[2], verts[6], verts[5], a_fillColour);

	// left
	addTri(verts[0], verts[1], verts[4], a_fillColour);
	addTri(verts[1], verts[5], verts[4], a_fillColour);

	// right
	addTri(verts[2], verts[3], verts[7], a_fillColour);
	addTri(verts[6], verts[2], verts[7], a_fillColour);
}

void Visualiser::addCylinderFilled(const Eigen::Vector4f& a_center, float a_radius, float a_halfLength,
	unsigned int a_segments, const Eigen::Vector4f& a_fillColour, const Eigen::Matrix4f* a_transform /* = nullptr */)
{
	Eigen::Vector4f white(1,1,1,1);

	float segmentSize = (2 * std::numbers::pi_v<float>) / a_segments;

	for ( unsigned int i = 0 ; i < a_segments ; ++i )
	{
		Eigen::Vector4f v0top(0,a_halfLength,0,0);
		Eigen::Vector4f v1top( sinf( i * segmentSize ) * a_radius, a_halfLength, cosf( i * segmentSize ) * a_radius,0 );
		Eigen::Vector4f v2top( sinf( (i+1) * segmentSize ) * a_radius, a_halfLength, cosf( (i+1) * segmentSize ) * a_radius,0 );
		Eigen::Vector4f v0bottom(0,-a_halfLength,0,0);
		Eigen::Vector4f v1bottom( sinf( i * segmentSize ) * a_radius, -a_halfLength, cosf( i * segmentSize ) * a_radius,0 );
		Eigen::Vector4f v2bottom( sinf( (i+1) * segmentSize ) * a_radius, -a_halfLength, cosf( (i+1) * segmentSize ) * a_radius ,0);

		if (a_transform != nullptr)
		{
			v0top = *a_transform * v0top;
			v1top = *a_transform * v1top;
			v2top = *a_transform * v2top;
			v0bottom = *a_transform * v0bottom;
			v1bottom = *a_transform * v1bottom;
			v2bottom = *a_transform * v2bottom;
		}

		// triangles
		addTri( a_center + v0top, a_center + v1top, a_center + v2top, a_fillColour);
		addTri( a_center + v0bottom, a_center + v2bottom, a_center + v1bottom, a_fillColour);
		addTri( a_center + v2top, a_center + v1top, a_center + v1bottom, a_fillColour);
		addTri( a_center + v1bottom, a_center + v2bottom, a_center + v2top, a_fillColour);

		// lines
		addLine(a_center + v1top, a_center + v2top, white, white);
		addLine(a_center + v1top, a_center + v1bottom, white, white);
		addLine(a_center + v1bottom, a_center + v2bottom, white, white);
	}
}

void Visualiser::addRing(const Eigen::Vector4f& a_center, float a_innerRadius, float a_outerRadius,
	unsigned int a_segments, const Eigen::Vector4f& a_fillColour, const Eigen::Matrix4f* a_transform /* = nullptr */)
{
	Eigen::Vector4f solidColour = a_fillColour;
	solidColour[3] = 1;

	float segmentSize = (2 * std::numbers::pi_v<float>) / a_segments;

	for ( unsigned int i = 0 ; i < a_segments ; ++i )
	{
		Eigen::Vector4f v1outer( sinf( i * segmentSize ) * a_outerRadius, 0, cosf( i * segmentSize ) * a_outerRadius, 0 );
		Eigen::Vector4f v2outer( sinf( (i+1) * segmentSize ) * a_outerRadius, 0, cosf( (i+1) * segmentSize ) * a_outerRadius, 0 );
		Eigen::Vector4f v1inner( sinf( i * segmentSize ) * a_innerRadius, 0, cosf( i * segmentSize ) * a_innerRadius, 0 );
		Eigen::Vector4f v2inner( sinf( (i+1) * segmentSize ) * a_innerRadius, 0, cosf( (i+1) * segmentSize ) * a_innerRadius, 0 );

		if (a_transform != nullptr)
		{
			v1outer = *a_transform * v1outer;
			v2outer = *a_transform * v2outer;
			v1inner = *a_transform * v1inner;
			v2inner = *a_transform * v2inner;
		}

		if (a_fillColour[3] != 0)
		{
			addTri(a_center + v1inner, a_center + v1outer, a_center + v2outer, a_fillColour);
			addTri(a_center + v2outer, a_center + v2inner, a_center + v1inner, a_fillColour);

			addTri(a_center + v2outer, a_center + v1outer, a_center + v1inner, a_fillColour);
			addTri(a_center + v1inner, a_center + v2inner, a_center + v2outer, a_fillColour);
		}
		else
		{
			// line
			addLine(a_center + v1inner + a_center, a_center + v2inner + a_center, solidColour, solidColour);
			addLine(a_center + v1outer + a_center, a_center + v2outer + a_center, solidColour, solidColour);
		}
	}
}

void Visualiser::addDisk(const Eigen::Vector4f& a_center, float a_radius,
	unsigned int a_segments, const Eigen::Vector4f& a_fillColour, const Eigen::Matrix4f* a_transform /* = nullptr */)
{
	Eigen::Vector4f vSolid = a_fillColour;
	vSolid[3] = 1;

	float fSegmentSize = (2 * std::numbers::pi_v<float>) / a_segments;

	for ( unsigned int i = 0 ; i < a_segments ; ++i )
	{
		Eigen::Vector4f v1outer( sinf( i * fSegmentSize ) * a_radius, 0, cosf( i * fSegmentSize ) * a_radius, 0 );
		Eigen::Vector4f v2outer( sinf( (i+1) * fSegmentSize ) * a_radius, 0, cosf( (i+1) * fSegmentSize ) * a_radius, 0 );

		if (a_transform != nullptr)
		{
			v1outer = *a_transform * v1outer;
			v2outer = *a_transform * v2outer;
		}

		if (a_fillColour[3] != 0)
		{
			addTri(a_center + v2outer, v1outer, a_center, a_fillColour);
			addTri(a_center, a_center + v1outer, a_center + v2outer, a_fillColour);
		}
		else
		{
			// line
			addLine(a_center + v1outer, a_center + v2outer, vSolid, vSolid);
		}
	}
}

void Visualiser::addArc(const Eigen::Vector4f& a_center,
	float a_radius, float a_arcHalfAngle, float a_rotation,
	unsigned int a_segments, const Eigen::Vector4f& a_fillColour, const Eigen::Matrix4f* a_transform /* = nullptr */)
{
	Eigen::Vector4f vSolid = a_fillColour;
	vSolid[3] = 1;

	float fSegmentSize = (2 * a_arcHalfAngle) / a_segments;

	for ( unsigned int i = 0 ; i < a_segments ; ++i )
	{
		Eigen::Vector4f v1outer( sinf( i * fSegmentSize - a_arcHalfAngle + a_rotation ) * a_radius, 0, cosf( i * fSegmentSize - a_arcHalfAngle + a_rotation ) * a_radius, 0 );
		Eigen::Vector4f v2outer( sinf( (i+1) * fSegmentSize - a_arcHalfAngle + a_rotation ) * a_radius, 0, cosf( (i+1) * fSegmentSize - a_arcHalfAngle + a_rotation ) * a_radius, 0 );

		if (a_transform != nullptr)
		{
			v1outer = *a_transform * v1outer;
			v2outer = *a_transform * v2outer;
		}

		if (a_fillColour[3] != 0)
		{
			addTri(a_center + v2outer, a_center + v1outer, a_center, a_fillColour);
			addTri(a_center, a_center + v1outer, a_center + v2outer, a_fillColour);
		}
		else
		{
			// line
			addLine(a_center + v1outer, a_center + v2outer, vSolid, vSolid);
		}
	}

	// edge lines
	if (a_fillColour[3] == 0)
	{
		Eigen::Vector4f v1outer( sinf( -a_arcHalfAngle + a_rotation ) * a_radius, 0, cosf( -a_arcHalfAngle + a_rotation ) * a_radius, 0 );
		Eigen::Vector4f v2outer( sinf( a_arcHalfAngle + a_rotation ) * a_radius, 0, cosf( a_arcHalfAngle + a_rotation ) * a_radius, 0 );

		if (a_transform != nullptr)
		{
			v1outer = *a_transform * v1outer;
			v2outer = *a_transform * v2outer;
		}

		addLine(a_center, a_center + v1outer, vSolid, vSolid);
		addLine(a_center, a_center + v2outer, vSolid, vSolid);
	}
}

void Visualiser::addArcRing(const Eigen::Vector4f& a_center,
	float a_innerRadius, float a_outerRadius, float a_arcHalfAngle, float a_rotation, 
	unsigned int a_segments, const Eigen::Vector4f& a_fillColour, const Eigen::Matrix4f* a_transform /* = nullptr */)
{
	Eigen::Vector4f vSolid = a_fillColour;
	vSolid[3] = 1;

	float fSegmentSize = (2 * a_arcHalfAngle) / a_segments;

	for ( unsigned int i = 0 ; i < a_segments ; ++i )
	{
		Eigen::Vector4f v1outer( sinf( i * fSegmentSize - a_arcHalfAngle + a_rotation ) * a_outerRadius, 0, cosf( i * fSegmentSize - a_arcHalfAngle + a_rotation ) * a_outerRadius, 0 );
		Eigen::Vector4f v2outer( sinf( (i+1) * fSegmentSize - a_arcHalfAngle + a_rotation ) * a_outerRadius, 0, cosf( (i+1) * fSegmentSize - a_arcHalfAngle + a_rotation ) * a_outerRadius, 0 );
		Eigen::Vector4f v1inner( sinf( i * fSegmentSize - a_arcHalfAngle + a_rotation  ) * a_innerRadius, 0, cosf( i * fSegmentSize - a_arcHalfAngle + a_rotation  ) * a_innerRadius, 0 );
		Eigen::Vector4f v2inner( sinf( (i+1) * fSegmentSize - a_arcHalfAngle + a_rotation  ) * a_innerRadius, 0, cosf( (i+1) * fSegmentSize - a_arcHalfAngle + a_rotation  ) * a_innerRadius, 0 );

		if (a_transform != nullptr)
		{
			v1outer = *a_transform * v1outer;
			v2outer = *a_transform * v2outer;
			v1inner = *a_transform * v1inner;
			v2inner = *a_transform * v2inner;
		}

		if (a_fillColour[3] != 0)
		{
			addTri(a_center + v2outer, a_center + v1outer, a_center + v1inner, a_fillColour);
			addTri(a_center + v1inner, a_center + v2inner, a_center + v2outer, a_fillColour);

			addTri(a_center + v1inner, a_center + v1outer, a_center + v2outer, a_fillColour);
			addTri(a_center + v2outer, a_center + v2inner, a_center + v1inner, a_fillColour);
		}
		else
		{
			// line
			addLine(a_center + v1inner, a_center + v2inner, vSolid, vSolid);
			addLine(a_center + v1outer, a_center + v2outer, vSolid, vSolid);
		}
	}

	// edge lines
	if (a_fillColour[3] == 0)
	{
		Eigen::Vector4f v1outer( sinf( -a_arcHalfAngle + a_rotation ) * a_outerRadius, 0, cosf( -a_arcHalfAngle + a_rotation ) * a_outerRadius, 0 );
		Eigen::Vector4f v2outer( sinf( a_arcHalfAngle + a_rotation ) * a_outerRadius, 0, cosf( a_arcHalfAngle + a_rotation ) * a_outerRadius, 0 );
		Eigen::Vector4f v1inner( sinf( -a_arcHalfAngle + a_rotation  ) * a_innerRadius, 0, cosf( -a_arcHalfAngle + a_rotation  ) * a_innerRadius, 0 );
		Eigen::Vector4f v2inner( sinf( a_arcHalfAngle + a_rotation  ) * a_innerRadius, 0, cosf( a_arcHalfAngle + a_rotation  ) * a_innerRadius, 0 );

		if (a_transform != nullptr)
		{
			v1outer = *a_transform * v1outer;
			v2outer = *a_transform * v2outer;
			v1inner = *a_transform * v1inner;
			v2inner = *a_transform * v2inner;
		}

		addLine(a_center + v1inner, a_center + v1outer, vSolid, vSolid);
		addLine(a_center + v2inner, a_center + v2outer, vSolid, vSolid);
	}
}

void Visualiser::addSphere(const Eigen::Vector4f& a_center, float a_radius, int a_rings, int a_segments, const Eigen::Vector4f& a_fillColour, 
								const Eigen::Matrix4f* a_transform /*= nullptr*/, float a_longitudeMin /*= 0.0f*/, float a_longitudeMax /*= pi<float>() * 2*/, 
								float a_latitudeMin /*= -half_pi<float>()*/, float a_latitudeMax /*= half_pi<float>()*/)
{
	float inverseRadius = 1 / a_radius;

	float invColumns = 1.0f / a_segments;
	float invRows = 1.0f / a_rings;
	
	//Lets put everything in radians first
	float latitiudinalRange = a_latitudeMax - a_latitudeMin;
	float longitudinalRange = a_longitudeMax - a_longitudeMin;

	// for each row of the mesh
	Eigen::Vector4f* v4Array = new Eigen::Vector4f[a_rings*a_segments + a_segments];

	for (int row = 0; row <= a_rings; ++row)
	{
		// y ordinates this may be a little confusing but here we are navigating around the xAxis in GL
		float ratioAroundXAxis = float(row) * invRows;
		float u_textureCoordinate = fabsf(1.0f-ratioAroundXAxis);
		float radiansAboutXAxis  = ratioAroundXAxis * latitiudinalRange + a_latitudeMin;
		float y  =  a_radius * sin(radiansAboutXAxis);
		float z  =  a_radius * cos(radiansAboutXAxis);
		
		for ( int col = 0; col <= a_segments; ++col )
		{
			float ratioAroundYAxis   = float(col) * invColumns;
			float v_textureCoordinate = fabsf(ratioAroundYAxis - 1.0f);
			float theta = ratioAroundYAxis * longitudinalRange + a_longitudeMin;
			Eigen::Vector4f v4Point = Eigen::Vector4f( -z * sinf(theta), y, -z * cosf(theta), 0 );
			Eigen::Vector4f v4Normal = Eigen::Vector4f( inverseRadius * v4Point[0], inverseRadius * v4Point[1], inverseRadius * v4Point[2], 0.f);

			if (a_transform != nullptr)
			{
				v4Point = *a_transform * v4Point;
				v4Normal = *a_transform * v4Normal;
			}
			Eigen::Vector2f v2TextureCoords = Eigen::Vector2f( u_textureCoordinate, v_textureCoordinate);
			int index = row * a_segments + (col % a_segments);
			v4Array[index] = v4Point;
		}
	}
	
	for (int face = 0; face < (a_rings)*(a_segments); ++face )
	{
		int iNextFace = face+1;
		
		
		if( iNextFace % a_segments == 0 )
		{
			iNextFace = iNextFace - (a_segments);
		}

		addLine(a_center + v4Array[face], a_center + v4Array[face+a_segments], Eigen::Vector4f(1.f,1.f,1.f,1.f), Eigen::Vector4f(1.f,1.f,1.f,1.f));
		
		if( face % a_segments == 0 && longitudinalRange < (2 * std::numbers::pi_v<float>))
		{
				continue;
		}
		addLine(a_center + v4Array[iNextFace+a_segments], a_center + v4Array[face+a_segments], Eigen::Vector4f(1.f,1.f,1.f,1.f), Eigen::Vector4f(1.f,1.f,1.f,1.f));

		addTri( a_center + v4Array[iNextFace+a_segments], a_center + v4Array[face], a_center + v4Array[iNextFace], a_fillColour);
		addTri( a_center + v4Array[iNextFace+a_segments], a_center + v4Array[face+a_segments], a_center + v4Array[face], a_fillColour);		
	}

	delete[] v4Array;	
}

void Visualiser::addHermiteSpline(const Eigen::Vector4f& a_rvStart, const Eigen::Vector4f& a_rvEnd,
	const Eigen::Vector4f& a_rvTangentStart, const Eigen::Vector4f& a_rvTangentEnd, unsigned int a_uiSegment, const Eigen::Vector4f& a_colour)
{
	a_uiSegment = a_uiSegment > 1 ? a_uiSegment : 1;

	Eigen::Vector4f vPrev = a_rvStart;

	for ( unsigned int i = 1 ; i <= a_uiSegment ; ++i )
	{
		float s = i / (float)a_uiSegment;

		float s2 = s * s;
		float s3 = s2 * s;
		float h1 = (2.0f * s3) - (3.0f * s2) + 1.0f;
		float h2 = (-2.0f * s3) + (3.0f * s2);
		float h3 =  s3- (2.0f * s2) + s;
		float h4 = s3 - s2;
		Eigen::Vector4f p = (a_rvStart * h1) + (a_rvEnd * h2) + (a_rvTangentStart * h3) + (a_rvTangentEnd * h4);
		p[3] = 1;

		addLine(vPrev,p,a_colour,a_colour);
		vPrev = p;
	}
}

void Visualiser::addLine(const Eigen::Vector4f& a_rv0,  const Eigen::Vector4f& a_rv1, 
	const Eigen::Vector4f& a_colour)
{
	addLine(a_rv0,a_rv1,a_colour,a_colour);
}

void Visualiser::addLine(const Eigen::Vector4f& a_v0, const Eigen::Vector4f& a_v1, 
	const Eigen::Vector4f& a_colour0, const Eigen::Vector4f& a_colour1)
{
	if (m_lineCount < m_maxLines)
	{
		m_lines[ m_lineCount ].v0.position = a_v0;
		m_lines[ m_lineCount ].v0.colour = a_colour0;
		m_lines[ m_lineCount ].v1.position = a_v1;
		m_lines[ m_lineCount ].v1.colour = a_colour1;

		++m_lineCount;
	}
}

void Visualiser::addTri(const Eigen::Vector4f& a_v0, const Eigen::Vector4f& a_v1, const Eigen::Vector4f& a_v2, const Eigen::Vector4f& a_colour)
{
	if (a_colour[3] == 1)
	{
		if (m_triCount < m_maxTris)
		{
			m_tris[ m_triCount ].v0.position = a_v0;
			m_tris[ m_triCount ].v1.position = a_v1;
			m_tris[ m_triCount ].v2.position = a_v2;
			m_tris[ m_triCount ].v0.colour = a_colour;
			m_tris[ m_triCount ].v1.colour = a_colour;
			m_tris[ m_triCount ].v2.colour = a_colour;

			++m_triCount;
		}
	}
	else
	{
		if (m_alphaTriCount < m_maxTris)
		{
			m_alphaTris[ m_alphaTriCount ].v0.position = a_v0;
			m_alphaTris[ m_alphaTriCount ].v1.position = a_v1;
			m_alphaTris[ m_alphaTriCount ].v2.position = a_v2;
			m_alphaTris[ m_alphaTriCount ].v0.colour = a_colour;
			m_alphaTris[ m_alphaTriCount ].v1.colour = a_colour;
			m_alphaTris[ m_alphaTriCount ].v2.colour = a_colour;

			++m_alphaTriCount;
		}
	}
}

void Visualiser::draw(const Eigen::Matrix4f& a_view, const Eigen::Matrix4f& a_projection)
{
	if (m_lineCount > 0 || m_triCount > 0 || m_alphaTriCount > 0)
	{
		glUseProgram(m_shaderProgram);

		glDisable( GL_DEPTH );
		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		GLuint ProjectionID = glGetUniformLocation(m_shaderProgram,"Projection");
		glUniformMatrix4fv(ProjectionID, 1, GL_FALSE, a_projection.data());

		GLuint ViewID = glGetUniformLocation(m_shaderProgram, "View");
		glUniformMatrix4fv(ViewID, 1, GL_FALSE, a_view.data());

		if (m_lineCount > 0)
		{
			glBindBuffer(GL_ARRAY_BUFFER, m_lineVBO);
			glBufferSubData(GL_ARRAY_BUFFER, 0, m_lineCount * sizeof(VisualiserLine), m_lines);

			glBindVertexArray(m_lineVAO);
			glDrawElements(GL_LINES, m_lineCount * 2, GL_UNSIGNED_INT, 0);
		}

		if (m_triCount > 0)
		{
			glBindBuffer(GL_ARRAY_BUFFER, m_triVBO);
			glBufferSubData(GL_ARRAY_BUFFER, 0, m_triCount * sizeof(VisualiserTri), m_tris);

			glBindVertexArray(m_triVAO);
			glDrawElements(GL_TRIANGLES, m_triCount * 3, GL_UNSIGNED_INT, 0);
		}

		if (m_alphaTriCount > 0)
		{
			glDepthMask(false);
			glBindBuffer(GL_ARRAY_BUFFER, m_alphaTriVBO);
			glBufferSubData(GL_ARRAY_BUFFER, 0, m_alphaTriCount * sizeof(VisualiserTri), m_tris);

			glBindVertexArray(m_alphaTriVAO);
			glDrawElements(GL_TRIANGLES, m_alphaTriCount * 3, GL_UNSIGNED_INT, 0);
			glDepthMask(true);
		}
		glDisable( GL_BLEND );
		glEnable( GL_DEPTH );
	}
}
