#include "Renderer_DX11.h"
#include <Profiler.h>
#include "SecretPointer.h"
namespace Graphics
{

	Renderer_DX11::Renderer_DX11(const RendererInitializationInfo & ii)
		: settings(ii), running(false), initiated(false), pipeline(nullptr), device(nullptr)
	{

	}


	Renderer_DX11::~Renderer_DX11()
	{
	}
	UERROR Renderer_DX11::Initialize()
	{
		StartProfile;

		if (initiated)
			RETURN_ERROR("Renderer has already been initiated");

		device = new DeviceHandler();
		if (!device)
			RETURN_ERROR("Could not create device handler");
		PASS_IF_ERROR(device->Init((HWND)settings.windowHandle, settings.windowState == WindowState::FULLSCREEN, settings.windowState == WindowState::FULLSCREEN_BORDERLESS, settings.bufferCount));
		
		pipeline = new PipelineAssigner();
		if (!device)
			RETURN_ERROR("Could not create pipeline");

		PASS_IF_ERROR(pipeline->Init(device->GetDevice(), device->GetDeviceContext(), 
			device->GetRTV(), device->GetSRV(),
			device->GetDepthStencil(), device->GetDepthSRV(),
			device->GetViewport()));

		initiated = true;
		renderer = this;
		RETURN_SUCCESS;
	}
	void Renderer_DX11::Shutdown()
	{
		StartProfile;
		if (running)
		{
			running = false;
			myThread.join();
		}
		if (pipeline)
		{
			pipeline->Shutdown();
			pipeline = nullptr;
		}
		if (device)
		{
			device->Shutdown();
			device = nullptr;
		}

		
		initiated = false;
		renderer = nullptr;
	}	

	float clearColor[5][4] = {
		{1, 0, 0, 1 },
		{ 1, 1, 0, 1 },
	{ 1, 0, 1, 1 },
	{ 0, 1, 0, 1 },
	{ 0, 0, 1, 1 }
	};
	int at = 0;
	void Renderer_DX11::Pause()
	{
	
		StartProfile;
		if (running)
		{
			running = false;
			myThread.join();
		}
		at = ++at % 5;
	}
	UERROR Renderer_DX11::Start()
	{
		StartProfile;
		if (running)
			RETURN_ERROR("Renderer already running");
		if (!initiated)
			RETURN_ERROR("Renderer must be initiated first");
		running = true;
		myThread = std::thread(&Renderer_DX11::Run, this);
		RETURN_SUCCESS;
	}
	UERROR Renderer_DX11::UpdateSettings(const RendererInitializationInfo & ii)
	{
		StartProfile;
		settings = ii;
		PASS_IF_ERROR(device->ResizeSwapChain((HWND)settings.windowHandle, settings.windowState == WindowState::FULLSCREEN, settings.windowState == WindowState::FULLSCREEN_BORDERLESS, settings.bufferCount));

		RETURN_SUCCESS;
	}
	const RendererInitializationInfo & Renderer_DX11::GetSettings() const
	{
		return settings;
	}
	PipelineHandler_Interface * Renderer_DX11::GetPipelineHandler()
	{
		return pipeline;
	}
	UERROR Renderer_DX11::AddRenderJob(Utilities::GUID id, const RenderJob & job, RenderGroup renderGroup)
	{
		StartProfile;

		if (renderJobs.clientSide.Find(id, renderGroup).has_value())
			RETURN_ERROR("RenderJob is already registered to group");

		renderJobs.clientSide.GetIDs(renderGroup).push_back(id);

		renderJobs.jobsToAdd.push({ id, renderGroup, job });


		RETURN_SUCCESS;
	}
	UERROR Renderer_DX11::AddRenderJob(const RenderJob & job, RenderGroup renderGroup)
	{
		return AddRenderJob(job.pipeline.ID(), job, renderGroup);
	}
	void Renderer_DX11::RemoveRenderJob(Utilities::GUID id, RenderGroup renderGroup)
	{
		StartProfile;

		if (auto index = renderJobs.clientSide.Find(id, renderGroup); index.has_value())
		{
			uint32_t last = uint32_t(renderJobs.clientSide.GetIDs(renderGroup).size()) - 1;
			renderJobs.clientSide.GetIDs(renderGroup)[*index] = renderJobs.clientSide.GetIDs(renderGroup)[last];
			renderJobs.clientSide.GetIDs(renderGroup).pop_back();

			renderJobs.jobsToRemove.push({ id, renderGroup });
		}
	}
	void Renderer_DX11::RemoveRenderJob(Utilities::GUID id)
	{
		StartProfile;

		for (uint8_t i = 0; i <= uint8_t(RenderGroup::FINAL_PASS); i++)
		{
			if (auto index = renderJobs.clientSide.Find(id, RenderGroup(i)); index.has_value())
			{
				uint32_t last = uint32_t(renderJobs.clientSide.renderGroupsWithID[i].size()) - 1;
				renderJobs.clientSide.renderGroupsWithID[i][*index] = renderJobs.clientSide.renderGroupsWithID[i][last];
				renderJobs.clientSide.renderGroupsWithID[i].pop_back();

				renderJobs.jobsToRemove.push({ id, RenderGroup(i) });
			}
		}
		
	}
	uint32_t Renderer_DX11::GetNumberOfRenderJobs() const
	{
		StartProfile;

		uint32_t count = 0;
		for (auto&g : renderJobs.clientSide.renderGroupsWithID)
			count += uint32_t(g.size());

		return count;
	}
	uint8_t Renderer_DX11::IsRenderJobRegistered(Utilities::GUID id) const
	{
		return renderJobs.clientSide.Registered(id);
	}
	UERROR Renderer_DX11::AddUpdateJob(Utilities::GUID id, const UpdateJob & job, RenderGroup renderGroupToPerformUpdateBefore)
	{
		StartProfile;
		if (auto find = updateJobs.clientSide.Find(id, renderGroupToPerformUpdateBefore); find.has_value())
			RETURN_ERROR("UpdateJob with this id already exists");

		if(job.frequency != UpdateFrequency::ONCE)
			updateJobs.clientSide.GetIDs(renderGroupToPerformUpdateBefore).push_back(id);

		updateJobs.jobsToAdd.push({ id, renderGroupToPerformUpdateBefore, job });
		RETURN_SUCCESS;
	}
	UERROR Renderer_DX11::AddUpdateJob(const UpdateJob & job, RenderGroup renderGroupToPerformUpdateBefore)
	{
		return AddUpdateJob(job.objectToMap, job, renderGroupToPerformUpdateBefore);
	}
	void Renderer_DX11::RemoveUpdateJob(Utilities::GUID id, RenderGroup renderGroup)
	{
		StartProfile;

		if (auto index = renderJobs.clientSide.Find(id, renderGroup); index.has_value())
		{
			uint32_t last = uint32_t(updateJobs.clientSide.GetIDs(renderGroup).size()) - 1;
			updateJobs.clientSide.GetIDs(renderGroup)[*index] = updateJobs.clientSide.GetIDs(renderGroup)[last];
			updateJobs.clientSide.GetIDs(renderGroup).pop_back();

			updateJobs.jobsToRemove.push({ id, renderGroup });
		}
	}
	void Renderer_DX11::RemoveUpdateJob(Utilities::GUID id)
	{
		StartProfile;

		for (uint8_t i = 0; i <= uint8_t(RenderGroup::FINAL_PASS); i++)
		{
			if (auto index = updateJobs.clientSide.Find(id, RenderGroup(i)); index.has_value())
			{
				uint32_t last = uint32_t(updateJobs.clientSide.renderGroupsWithID[i].size()) - 1;
				updateJobs.clientSide.renderGroupsWithID[i][*index] = updateJobs.clientSide.renderGroupsWithID[i][last];
				updateJobs.clientSide.renderGroupsWithID[i].pop_back();

				updateJobs.jobsToRemove.push({ id, RenderGroup(i) });
			}
		}
	}
	uint32_t Renderer_DX11::GetNumberOfUpdateJobs() const
	{
		StartProfile;

		uint32_t count = 0;
		for (auto&g : updateJobs.clientSide.renderGroupsWithID)
			count += uint32_t(g.size());

		return count;
	}
	uint8_t Renderer_DX11::IsUpdateJobRegistered(Utilities::GUID id) const
	{
		return updateJobs.clientSide.Registered(id);
	}
	void Renderer_DX11::Run()
	{
		StartProfile;
		while (running)
		{
			StartProfileC("Run_While");
			ResolveJobs();

			clearColor[at][0] =fmodf(clearColor[at][0] + 0.01f, 1.0f);

			BeginFrame();

			Frame();

			EndFrame();
		}
	}
	void Renderer_DX11::ResolveJobs()
	{
		StartProfile;
		{
			pipeline->UpdatePipelineObjects();

			{
				StartProfileC("Remove Jobs");
				updateJobs.Remove();
				renderJobs.Remove();
			}
			{
				StartProfileC("Add Jobs");
				updateJobs.Add();
				renderJobs.Add();
			}
		}
	}
	void Renderer_DX11::BeginFrame()
	{
		StartProfile;
		ID3D11RenderTargetView* views[] = { device->GetRTV() };
		device->GetDeviceContext()->OMSetRenderTargets(1, views, device->GetDepthStencil());

		// Clear the primary render target view using the specified color
		device->GetDeviceContext()->ClearRenderTargetView(device->GetRTV(), clearColor[at]);

		// Clear the standard depth stencil view
		device->GetDeviceContext()->ClearDepthStencilView(device->GetDepthStencil(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	}
	void Renderer_DX11::Frame()
	{
		StartProfile;
		static std::vector<JobStuff<UpdateJob>::ToRemove> updateJobsToRemove;
		for (uint8_t i = 0; i <= uint8_t(RenderGroup::FINAL_PASS); i++)
		{
			auto& uj = updateJobs.renderSide.GetJobs(RenderGroup(i));
			for (auto& job : uj)
			{
				job.updateCallback(pipeline->GetUpdateObject(job.objectToMap, job.type));
				if (job.frequency == UpdateFrequency::ONCE)
					updateJobsToRemove.push_back({ job.objectToMap,RenderGroup(i) });
			}

		
		}

		for (auto toRemove : updateJobsToRemove)
			updateJobs.renderSide.Remove(toRemove);
	}
	void Renderer_DX11::EndFrame()
	{
		StartProfile;
		device->Present(settings.vsync);
	}

}