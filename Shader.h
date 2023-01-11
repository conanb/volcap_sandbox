#pragma once

#include <map>
#include <vector>
#include <memory>
#include <string>

#include <Eigen/Core>

class Shader {
	friend class UniformBase;

public:

	Shader(const std::string& a_name);
	virtual ~Shader();

	auto		getName() const	{	return m_name;	}

	enum class Stage : unsigned int
	{
		Vertex,
		Control,
		Evaluation,
		Geometry,
		Fragment,

		Count,
	};

	void	reload();

	bool	compileShaderFromFile(Stage a_type, const std::string& a_filename, bool a_storeFile = true);
	bool	compileShaderFromString(Stage a_type, const char* a_shader);

	bool	linkProgram();

	bool	buildProgram(const char** a_shaderFiles);

	void	bind();
	void	unBind();

	bool	isBound() const;

	unsigned int	getProgram() const;
	
	// Uniforms
	class UniformBase
	{
		friend class Shader;

	public:

		const std::string&	getName() const	{	return m_name;		}
		Shader*			getShader() const	{	return m_shader;	}
		int				getLocation() const	{	return m_location;	}
		unsigned int	getSize() const		{	return m_size;		}
		unsigned int	getType() const		{	return m_type;		}

		void			bind(float f);
		void			bind(int i);
		void			bind(unsigned int u);
		void			bind(const Eigen::Vector2f& v);
		void			bind(const Eigen::Vector3f& v);
		void			bind(const Eigen::Vector4f& v);
		void			bind(const Eigen::Vector2i& iv);
		void			bind(const Eigen::Vector3i& iv);
		void			bind(const Eigen::Vector4i& iv);
		void			bind(const Eigen::Vector<unsigned int, 2>& iv);
		void			bind(const Eigen::Vector<unsigned int, 3>& iv);
		void			bind(const Eigen::Vector<unsigned int, 4>& iv);
		void			bind(const Eigen::Matrix2f& m);
		void			bind(const Eigen::Matrix3f& m);
		void			bind(const Eigen::Matrix4f& m);

		void			bind(unsigned int a_count, float* f);
		void			bind(unsigned int a_count, int* i);
		void			bind(unsigned int a_count, unsigned int* u);
		void			bind(unsigned int a_count, const Eigen::Vector2f* iv);
		void			bind(unsigned int a_count, const Eigen::Vector3f* iv);
		void			bind(unsigned int a_count, const Eigen::Vector4f* iv);
		void			bind(unsigned int a_count, const Eigen::Vector2i* iv);
		void			bind(unsigned int a_count, const Eigen::Vector3i* iv);
		void			bind(unsigned int a_count, const Eigen::Vector4i* iv);
		void			bind(unsigned int a_count, const Eigen::Vector<unsigned int, 2>* iv);
		void			bind(unsigned int a_count, const Eigen::Vector<unsigned int, 3>* iv);
		void			bind(unsigned int a_count, const Eigen::Vector<unsigned int, 4>* iv);
		void			bind(unsigned int a_count, const Eigen::Matrix2f* m);
		void			bind(unsigned int a_count, const Eigen::Matrix3f* m);
		void			bind(unsigned int a_count, const Eigen::Matrix4f* m);

	private:

		UniformBase(const char* a_name, unsigned int a_location, unsigned int a_size, unsigned int a_type, Shader* a_shader) 
			: m_dirty(true), m_name(a_name), m_location(a_location), m_size(a_size), m_type(a_type), m_shader(a_shader)
		{
			registerDirty();
		}
		virtual ~UniformBase() = default;

		void			registerDirty();
		virtual void	clean() = 0;

		bool			m_dirty;
		std::string		m_name;
		int				m_location;
		unsigned int	m_type;
		unsigned int	m_size;
		Shader*			m_shader;
	};

	
	template <typename T>
	class Uniform : public UniformBase
	{
		friend class Shader;

	public:

		void	set(const T& a_value);
		void	set(unsigned int a_count, const T* a_values);

	private:

		virtual void	clean();

		Uniform(const char* a_name, unsigned int a_location, unsigned int a_size, unsigned int a_type, Shader* a_owner) 
			: UniformBase(a_name, a_location, a_size, a_type, a_owner)
		{
			m_value = new T[a_size];
		}
		virtual ~Uniform() 
		{
			delete[] m_value;
		}
		
		T*				m_value;
	};

	UniformBase*	getUniform(const std::string& a_uniform);
	
	// shared uniforms
/*	class SharedUniformBase
	{
	public:

		SharedUniformBase(UniformBase* a_uniform) : m_uniform(a_uniform) {}
		virtual ~SharedUniformBase() {}

		virtual void set() = 0;

	protected:

		UniformBase*	m_uniform;
	};

	class SharedUniformFactoryBase
	{
	public:

		virtual ~SharedUniformFactoryBase() {}
		virtual SharedUniformBase* create(UniformBase* a_uniform) = 0;
	};

	template <typename T>
	class SharedUniformFactory : public SharedUniformFactoryBase
	{
	public:

		virtual ~SharedUniformFactory() {}
		virtual SharedUniformBase* create(UniformBase* a_uniform)
		{
			return new T(a_uniform);
		}
	};*/

//	static void		registerSharedUniform( const char* a_name, SharedUniformFactoryBase* a_factory );

//	static void		initialiseSharedUniforms();
//	static void		destroySharedUniforms();
	
	// not the preferred way to set uniforms
/*	void			setUniform(const std::string& a_uniform, int i);
	void			setUniform(const std::string& a_uniform, float f);
	void			setUniform(const std::string& a_uniform, const Eigen::Vector2f& v);
	void			setUniform(const std::string& a_uniform, const Eigen::Vector3f& v);
	void			setUniform(const std::string& a_uniform, const Eigen::Vector4f& v);
	void			setUniform(const std::string& a_uniform, const Eigen::Vector2i& v);
	void			setUniform(const std::string& a_uniform, const Eigen::Vector3i& v);
	void			setUniform(const std::string& a_uniform, const Eigen::Vector4i& v);
	void			setUniform(const std::string& a_uniform, const Eigen::Matrix2f& m);
	void			setUniform(const std::string& a_uniform, const Eigen::Matrix3f& m);
	void			setUniform(const std::string& a_uniform, const Eigen::Matrix4f& m);
	void			setUniform(const std::string& a_uniform, unsigned int a_count, int* i);
	void			setUniform(const std::string& a_uniform, unsigned int a_count, float* f);
	void			setUniform(const std::string& a_uniform, unsigned int a_count, const Eigen::Vector2f* v);
	void			setUniform(const std::string& a_uniform, unsigned int a_count, const Eigen::Vector3f* v);
	void			setUniform(const std::string& a_uniform, unsigned int a_count, const Eigen::Vector4f* v);
	void			setUniform(const std::string& a_uniform, unsigned int a_count, const Eigen::Vector2i* v);
	void			setUniform(const std::string& a_uniform, unsigned int a_count, const Eigen::Vector3i* v);
	void			setUniform(const std::string& a_uniform, unsigned int a_count, const Eigen::Vector4i* v);
	void			setUniform(const std::string& a_uniform, unsigned int a_count, const Eigen::Matrix2f* m);
	void			setUniform(const std::string& a_uniform, unsigned int a_count, const Eigen::Matrix3f* m);
	void			setUniform(const std::string& a_uniform, unsigned int a_count, const Eigen::Matrix4f* m);*/

	static Shader* 		getBoundShader();

	static const char*	getLastError();

private:

	void			extractUniforms();
	void			registerDirty(UniformBase* a_uniform);

	// adds #include support
	bool			readShaderSource( const std::string& a_filename, std::string& a_source );

	std::string		m_name;

	unsigned int	m_program;
	unsigned int	m_shaders[(unsigned int)Stage::Count];
	char*			m_shaderFiles[(unsigned int)Stage::Count];

	std::map< unsigned int, UniformBase* >	m_uniforms;
	std::vector< UniformBase* >				m_dirtyUniforms;

//	std::vector< SharedUniformBase* >							m_sharedUniforms;
//	static std::map< unsigned int, SharedUniformFactoryBase* >	sm_sharedUniformFactories;

	static char		sm_errorLog[2048];
	static Shader*	sm_boundShader;
};

inline bool Shader::isBound() const
{
	return this == sm_boundShader;
}

inline Shader* Shader::getBoundShader()
{
	return sm_boundShader;
}

inline const char* Shader::getLastError()
{
	return sm_errorLog;
}

inline void Shader::registerDirty(UniformBase* a_uniform)
{
	m_dirtyUniforms.push_back(a_uniform);
}

inline void Shader::UniformBase::registerDirty()
{
	m_dirty = true;
	m_shader->registerDirty(this);
}

template <typename T>
inline void Shader::Uniform<T>::set(const T& a_value)
{
	if (m_dirty == false &&
		m_value[0] != a_value)
	{
		registerDirty();
	}
	m_value[0] = a_value;
}

template <typename T>
inline void Shader::Uniform<T>::set(unsigned int a_count, const T* a_values)
{
	if (m_dirty == false &&
		memcmp(m_value,a_values,a_count * sizeof(T)) > 0)
	{
		registerDirty();
	}
	memcpy(m_value,a_values,a_count * sizeof(T));
}

template <typename T>
inline void Shader::Uniform<T>::clean()
{
	if (m_size > 1)
		bind(m_size,m_value);
	else
		bind(m_value[0]);
	m_dirty = false;
}

inline unsigned int Shader::getProgram() const
{
	return m_program;
}