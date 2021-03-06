#ifndef SE_GRAPHICS_UPDATE_JOB_H_
#define SE_GRAPHICS_UPDATE_JOB_H_
#include "Pipeline.h"
#include <functional>
#include <vector>
#include "RenderJob.h"
#include <Error.h>

#ifndef ENUM_FLAG_OPERATOR
#define ENUM_FLAG_OPERATOR(T,X) inline T operator X (T lhs, T rhs) { return (T) (static_cast<std::underlying_type_t <T>>(lhs) X static_cast<std::underlying_type_t <T>>(rhs)); } 
#endif
#ifndef ENUM_FLAG_OPERATOR2
#define ENUM_FLAG_OPERATOR2(T,X) inline void operator X= (T& lhs, T rhs) { lhs = (T)(static_cast<std::underlying_type_t <T>>(lhs) X static_cast<std::underlying_type_t <T>>(rhs)); } 
#endif
#ifndef ENUM_FLAGS
#define ENUM_FLAGS(T) \
inline T operator ~ (T t) { return (T) (~static_cast<std::underlying_type_t <T>>(t)); } \
inline bool operator & (T lhs, T rhs) { return (static_cast<std::underlying_type_t <T>>(lhs) & static_cast<std::underlying_type_t <T>>(rhs));  } \
ENUM_FLAG_OPERATOR2(T,|) \
ENUM_FLAG_OPERATOR2(T,&) \
ENUM_FLAG_OPERATOR(T,|) \
ENUM_FLAG_OPERATOR(T,^)
#endif

namespace Graphics
{
	enum class UpdateFrequency : uint8_t
	{
		EVERY_FRAME,
		ONCE
	};
	enum class AccessFlag
	{
		READ = 1 << 0,
		WRITE = 1 << 1
	};

	ENUM_FLAGS(Graphics::AccessFlag);

	struct UpdateObject
	{
		virtual ~UpdateObject(){}

		template<class T>
		T& GetMapObject(AccessFlag flag = AccessFlag::READ)
		{
			void* data = nullptr;
			return *(T*)Map(flag);
		}
		virtual void* Map(AccessFlag flag = AccessFlag::READ)
		{
			THROW_ERROR("Map not implemented");
		}
		virtual void Unmap() {}
		template<class T>
		const T& GetInfo()const
		{
			return *(T*)GetInfo_();
		}

		virtual UERROR WriteTo(void*data, size_t size)
		{
			RETURN_ERROR("WriteTo not implemented");
		}
		virtual UERROR ReadFrom(void*data, size_t size)
		{
			RETURN_ERROR("ReadFrom not implemented");
		}
	protected:
		virtual const void* GetInfo_()const
		{
			THROW_ERROR("GetInfo_ not implemented");
		}
	};


	enum class PipelineObjectType
	{
		Buffer,

		RenderTarget,
		UnorderedAccessView,
		Texture,
		DepthStencilView,

		Viewport
	};



	struct UpdateJob
	{
		Utilities::GUID objectToMap;
		UpdateFrequency frequency;
		PipelineObjectType type;
		std::function<UERROR(UpdateObject* obj)> updateCallback;

		static UpdateJob Buffer(Utilities::GUID id, UpdateFrequency freq, const std::function<UERROR(UpdateObject* obj)>& cb)
		{
			return { id, freq, PipelineObjectType::Buffer, cb };
		}
	};
}


#endif
