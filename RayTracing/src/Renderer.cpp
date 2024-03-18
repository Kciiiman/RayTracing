#include "Renderer.h"
#include "Walnut//Random.h"
#include <execution>
namespace Utils {

	static uint32_t ConvertToRGBA(glm::vec4 color)
	{
		uint8_t r = (uint8_t)(color.r * 255.0f);
		uint8_t g = (uint8_t)(color.g * 255.0f);
		uint8_t b = (uint8_t)(color.b * 255.0f);
		uint8_t a = (uint8_t)(color.a * 255.0f);

		uint32_t result = (a << 24) | (b << 16) | (g << 8) | r;
		return result;
	}

	static uint32_t PCG_Hash(uint32_t input)
	{
		uint32_t state = input * 747796405u + 2891336453u;
		uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
		return (word >> 22u) ^ word;
	}

	static float RandomFloat(uint32_t& seed)
	{
		seed = PCG_Hash(seed);
		return (float)seed / (float)std::numeric_limits<uint32_t>::max();
	}

	static glm::vec3 InUnitSphere(uint32_t& seed)
	{
		return glm::normalize(glm::vec3(
			RandomFloat(seed) * 2.0f - 1.0f,
			RandomFloat(seed) * 2.0f - 1.0f,
			RandomFloat(seed) * 2.0f - 1.0f
		));
	}
}


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

	delete[] m_AccumulationData;
	m_AccumulationData = new glm::vec4[width * height];

	m_ImageHorizontalIter.resize(width);
	m_ImageVerticalIter.resize(height);
	for (uint32_t i = 0; i < width; i++)
		m_ImageHorizontalIter[i] = i;
	for (uint32_t i = 0; i < height; i++)
		m_ImageVerticalIter[i] = i;

}

void Renderer::Render(const Scene& scene,const Camera& camera)
{	
	m_ActiveScene = &scene;
	m_ActiveCamera = &camera;


	if (m_FrameIndex == 1)
		memset(m_AccumulationData, 0, m_FinalImage->GetWidth() * m_FinalImage->GetHeight() * sizeof(glm::vec4));

#define MT 1
#if MT
	std::for_each(std::execution::par, m_ImageVerticalIter.begin(), m_ImageVerticalIter.end(),
		[this](uint32_t y)
		{
			std::for_each(std::execution::par, m_ImageHorizontalIter.begin(), m_ImageHorizontalIter.end(),
			[this, y](uint32_t x)
				{
					glm::vec4 color = PerPixel(x, y);
					m_AccumulationData[x + y * m_FinalImage->GetWidth()] += color;
					glm::vec4 AccumulateColor = m_AccumulationData[x + y * m_FinalImage->GetWidth()];
					AccumulateColor /= (float)m_FrameIndex;

					AccumulateColor = glm::clamp(AccumulateColor, glm::vec4(0.0f), glm::vec4(1.0f));
					m_ImageData[x + y * m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(AccumulateColor);
				});
		});
#else
	for (uint32_t y = 0; y < m_FinalImage->GetHeight(); y++)
	{
		for (uint32_t x = 0; x < m_FinalImage->GetWidth(); x++)
		{	
			glm::vec4 color = PerPixel(x, y);
			m_AccumulationData[x + y * m_FinalImage->GetWidth()] += color;
			glm::vec4 AccumulateColor = m_AccumulationData[x + y * m_FinalImage->GetWidth()];
			AccumulateColor /= (float)m_FrameIndex;

			AccumulateColor = glm::clamp(AccumulateColor, glm::vec4(0.0f), glm::vec4(1.0f));
			m_ImageData[x + y* m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(AccumulateColor);
		}
	}
#endif 

	m_FinalImage->SetData(m_ImageData);

	if (m_Settings.Accumulate)
		m_FrameIndex++;
	else
		m_FrameIndex = 1;
}

glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y)
{
	Ray ray;
	ray.Origin = m_ActiveCamera->GetPosition();
	ray.Direction = m_ActiveCamera->GetRayDirections()[x + y * m_FinalImage->GetWidth()];

	glm::vec3 light = glm::vec3(0.0f,0.0f,0.0f);
	int bounce = 5;
	glm::vec3 contribution(1.0f);
	uint32_t seed = x + y * m_FinalImage->GetWidth();
	seed *= m_FrameIndex;
	for(int i =0 ; i<bounce; i++)
	{	
		seed += i;
		Renderer::HitPayload payload = TraceRay(ray);
		if (payload.HitDistance < 0.0f)
		{
			glm::vec3 skyColor = glm::vec3(0.6f, 0.7f, 0.9f);
			//light += skyColor * contribution;
			break;
		}
		
		const Sphere& sphere = m_ActiveScene->Spheres[payload.ObjectIndex];
		const Material& material = m_ActiveScene->Materials[sphere.MaterialIndex];

		contribution *= material.Albedo; 
		light += material.GetEmission();

		ray.Origin = payload.WorldPosition + payload.WorldNormal * 0.0001f;
		//ray.Direction = glm::reflect(ray.Direction,
			//payload.WorldNormal + material.Roughness * Walnut::Random::Vec3(-0.5f,0.5f));
		if(m_Settings.SlowRender)
			ray.Direction = glm::normalize(payload.WorldNormal + Walnut::Random::InUnitSphere());
		else
			ray.Direction = glm::normalize(payload.WorldNormal + Utils::InUnitSphere(seed));
	}

	return glm::vec4(light,1.0f);
}

Renderer::HitPayload Renderer::ClosestT(const Ray& ray, float HitDistance, int ObjectIndex)
{	
	Renderer::HitPayload payload;
	payload.HitDistance = HitDistance;
	payload.ObjectIndex = ObjectIndex;	//最近点索引更新

	glm::vec3 Origin = ray.Origin - m_ActiveScene->Spheres[ObjectIndex].Position;
	payload.WorldPosition = Origin + ray.Direction * HitDistance;
	payload.WorldNormal = glm::normalize(payload.WorldPosition);
	payload.WorldPosition += m_ActiveScene->Spheres[ObjectIndex].Position;
		
	return payload;
}

Renderer::HitPayload Renderer::Miss(const Ray& ray)
{	
	Renderer::HitPayload payload;
	payload.HitDistance = -1.0f;
	return payload;
}

Renderer::HitPayload Renderer::TraceRay(const Ray& ray)
{	 
	//(bx^2 + by^2)t^2 + (2(axbx + ayby))t + (ax^2 + ay^2 - r^2) = 0
	//a = ray origin
	//b = ray direction
	//r = radius
	//t = hit distance

	int closestSphere = -1;
	float hitDistance = std::numeric_limits<float>::max();
	for (int i = 0; i<m_ActiveScene->Spheres.size(); i++)
	{	
		const Sphere& sphere = m_ActiveScene->Spheres[i];
		//计算一元二次里的a,b,c
		glm::vec3 Origin = ray.Origin - sphere.Position;
		float a = glm::dot(ray.Direction, ray.Direction);
		float b = 2.0f * glm::dot(Origin, ray.Direction);
		float c = glm::dot(Origin, Origin) - sphere.Radius * sphere.Radius;

		//判断交点个数
		//b^2 - 4ac
		//（-b +- sqrt(discriminant)）/(2.0f * a)
		float discriminat = b * b - 4.0f * a * c;

		if (discriminat < 0.0f)
			continue;

		//float t0 = (-b + glm::sqrt(discriminat)) / (2.0f * a);
		float closesetT = (-b - glm::sqrt(discriminat)) / (2.0f * a);
		if(closesetT > 0.0f && closesetT < hitDistance)
		{
			hitDistance = closesetT;
			closestSphere = (int)i;
		}
	}

	if (closestSphere < 0)
	{
		return Miss(ray);
	}

	return ClosestT(ray, hitDistance, closestSphere);
}
