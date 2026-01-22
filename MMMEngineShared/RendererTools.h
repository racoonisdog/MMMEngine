#pragma once
#include <d3d11_4.h>
#include <cwchar>
#include <exception>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <vector>
#include <SimpleMath.h>

namespace MMMEngine {
	HRESULT CompileShaderFromFile(const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut);

	class com_exception : public std::exception
	{
	public:
		com_exception(HRESULT hr) : result(hr) {}

		const char* what() const noexcept override
		{
			static char s_str[64] = {};
			sprintf_s(s_str, "Failure with HRESULT of %08X",
				static_cast<unsigned int>(result));
			return s_str;
		}

	private:
		HRESULT result;
	};

	inline void HR_T(HRESULT hr)
	{
		if (FAILED(hr))
		{
			throw com_exception(hr);
		}
	}
}


