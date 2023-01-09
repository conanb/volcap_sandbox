#pragma once

#include <Eigen/Core>
#include <numbers>

class Gizmos
{
public:

	static Gizmos*	create(unsigned int a_maxLines = 32768, unsigned int a_maxTris = 32768);
	static Gizmos*	getSingleton()	{	return sm_singleton;	}
	static void			destroy();

	// removes all visualisers
	void			clear();

	// draws current visualiser buffers
	void			draw(const Eigen::Matrix4f& a_view, const Eigen::Matrix4f& a_projection);

	// Adds a single debug line
	void			addLine(const Eigen::Vector4f& a_v0,  const Eigen::Vector4f& a_v1, 
							const Eigen::Vector4f& a_colour);

	// Adds a single debug line
	void			addLine(const Eigen::Vector4f& a_v0, const Eigen::Vector4f& a_v1, 
							const Eigen::Vector4f& a_colour0, const Eigen::Vector4f& a_colour1);

	// Adds a triangle.
	void			addTri(const Eigen::Vector4f& a_v0, const Eigen::Vector4f& a_v1, const Eigen::Vector4f& a_v2, const Eigen::Vector4f& a_colour);

	// Adds 3 unit-length lines (red,green,blue) representing the 3 axis of a transform, 
	// at the transform's translation. Optional scale available.
	void			addTransform(const Eigen::Matrix4f& a_transform, float a_scale = 1.0f);
	
	// Adds a wireframe Axis-Aligned Bounding-Box with optional transform for rotation/translation.
	void			addAABB(const Eigen::Vector4f& a_center, const Eigen::Vector4f& a_extents, 
							const Eigen::Vector4f& a_colour, const Eigen::Matrix4f* a_transform = nullptr);

	// Adds an Axis-Aligned Bounding-Box with optional transform for rotation.
	void			addAABBFilled(const Eigen::Vector4f& a_center, const Eigen::Vector4f& a_extents, 
								const Eigen::Vector4f& a_fillColour, const Eigen::Matrix4f* a_transform = nullptr);

	// Adds a cylinder aligned to the Y-axis with optional transform for rotation.
	void			addCylinderFilled(const Eigen::Vector4f& a_center, float a_radius, float a_halfLength,
									unsigned int a_segments, const Eigen::Vector4f& a_fillColour, const Eigen::Matrix4f* a_transform = nullptr);

	// Adds a double-sided hollow ring in the XZ axis with optional transform for rotation.
	// If a_rvFilLColour.w == 0 then only an outer and inner line is drawn.
	void			addRing(const Eigen::Vector4f& a_center, float a_innerRadius, float a_outerRadius,
							unsigned int a_uiSegments, const Eigen::Vector4f& a_rvFillColour, const Eigen::Matrix4f* a_transform = nullptr);

	// Adds a double-sided disk in the XZ axis with optional transform for rotation.
	// If a_rvFilLColour.w == 0 then only an outer line is drawn.
	void			addDisk(const Eigen::Vector4f& a_center, float a_radius,
							unsigned int a_segments, const Eigen::Vector4f& a_fillColour, const Eigen::Matrix4f* a_transform = nullptr);

	// Adds an arc, around the Y-axis
	// If a_rvFilLColour.w == 0 then only an outer line is drawn.
	void			addArc(const Eigen::Vector4f& a_center, float a_radius, float a_arcHalfAngle, float a_rotation,
							unsigned int a_segments, const Eigen::Vector4f& a_fillColour, const Eigen::Matrix4f* a_transform = nullptr);

	// Adds an arc, around the Y-axis, starting at the inner radius and extending to the outer radius
	// If a_rvFilLColour.w == 0 then only an outer line is drawn.
	void			addArcRing(const Eigen::Vector4f& a_center, float a_innerRadius, float a_outerRadius, float a_arcHalfAngle, float a_rotation, 
								unsigned int a_segments, const Eigen::Vector4f& a_fillColour, const Eigen::Matrix4f* a_transform = nullptr);

	// Adds a Sphere at a given position, with a given number of rows, and columns, radius and a max and min long and latitude
	void			addSphere(const Eigen::Vector4f& a_center, float a_radius, int a_rings, int a_segments, const Eigen::Vector4f& a_fillColour, 
								const Eigen::Matrix4f* a_transform = nullptr, float a_longitudeMin = 0.0f, float a_longitudeMax = std::numbers::pi_v<float> * 2,
								float a_latitudeMin = -std::numbers::pi_v<float> * 0.5f, float a_latitudeMax = std::numbers::pi_v<float> * 0.5f );

	// Adds a single Hermite spline curve
	void			addHermiteSpline(const Eigen::Vector4f& a_start, const Eigen::Vector4f& a_end,
									const Eigen::Vector4f& a_tangentStart, const Eigen::Vector4f& a_tangentEnd, unsigned int a_segments, const Eigen::Vector4f& a_colour);
	
private:

	Gizmos(unsigned int a_maxLines, unsigned int a_maxTris);
	~Gizmos();

	struct VisualiserVertex
	{
		Eigen::Vector4f position;
		Eigen::Vector4f colour;
	};

	struct VisualiserLine
	{
		VisualiserVertex v0;
		VisualiserVertex v1;
	};

	struct VisualiserTri
	{
		VisualiserVertex v0;
		VisualiserVertex v1;
		VisualiserVertex v2;
	};

	unsigned int	m_shaderProgram;
	unsigned int	m_vertexShader;
	unsigned int	m_fragmentShader;

	// line data
	unsigned int	m_maxLines;
	unsigned int	m_lineCount;
	VisualiserLine*	m_lines;

	unsigned int	m_lineVAO;
	unsigned int 	m_lineVBO;
	unsigned int 	m_lineIBO;

	// triangle data
	unsigned int	m_maxTris;
	unsigned int	m_triCount;
	VisualiserTri*	m_tris;

	unsigned int	m_triVAO;
	unsigned int 	m_triVBO;
	unsigned int 	m_triIBO;

	// triangle data
	unsigned int	m_alphaTriCount;
	VisualiserTri*	m_alphaTris;

	unsigned int	m_alphaTriVAO;
	unsigned int 	m_alphaTriVBO;
	unsigned int 	m_alphaTriIBO;

	static Gizmos*	sm_singleton;
};
