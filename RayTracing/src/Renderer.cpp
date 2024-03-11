#include "Renderer.h"
#include "Walnut//Random.h"

void Renderer::OnResize(uint32_t width, uint32_t height)
{	
	if(m_FinalImage)
	{
		if (m_FinalImage->GetWidth() == width && m_FinalImage->GetHeight() == height)
			return;

		m_FinalImage->Resize(width, height);
	}
	else
	{
		m_FinalImage = std::make_shared<Walnut::Image>(width, height, Walnut::ImageFormat::RGBA);
	}
	delete[] m_ImageData;
	m_ImageData = new uint32_t[width * height];

}

//渲染每一个像素
void Renderer::Render()
{	
	for (uint32_t y = 0; y < m_FinalImage->GetHeight(); y++)
	{
		for (uint32_t x = 0; x < m_FinalImage->GetWidth(); x++)
		{	
			glm::vec2 coord = { x / (float)m_FinalImage->GetWidth(), y / (float)m_FinalImage->GetHeight() };	//转换成（0，1）区间
			coord = coord * 2.0f - 1.0f; // -1 -> 1
			m_ImageData[x + y* m_FinalImage->GetWidth()] = PerPixel(coord);
		}
	}

	m_FinalImage->SetData(m_ImageData);
}

uint32_t Renderer::PerPixel(glm::vec2 coord)
{	 
	glm::vec3 rayOrigin(0.0f,0.0f,1.0f);
	glm::vec3 rayDirection(coord.x, coord.y, -1.0f);
	float radius = 0.5f;
	//rayDirection = glm::normalize(rayDirection);

	//(bx^2 + by^2)t^2 + (2(axbx + ayby))t + (ax^2 + ay^2 - r^2) = 0
	//a = ray origin
	//b = ray direction
	//r = radius
	//t = hit distance

	//计算一元二次里的a,b,c
	float a = glm::dot(rayDirection, rayDirection);
	float b = 2.0f * glm::dot(rayOrigin, rayDirection);
	float c = glm::dot(rayOrigin, rayOrigin) - radius * radius;

	//判断交点个数
	//b^2 - 4ac
	float discriminat = b * b - 4.0f * a * c;
	if (discriminat >= 0.0f)
		return 0xffff00ff;

	return 0xff000000;
}
