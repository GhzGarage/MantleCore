// Copyright Joshua Eger. All Rights Reserved.
#include "MantleFaceLayout.h"

#include "MantleBodyCorrespondence.h"
#include "MantleBodyTopologyData.h"

#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogMantleFaceLayout, Log, All);

// Named, not anonymous: MantleEditor builds as a unity build.
namespace MantleFaceLayoutPrivate
{
	bool Fail(FString& Error, const FString& Reason)
	{
		Error = Reason;
		return false;
	}

	/** "head 3401v/6000t, teeth 300v/500t" */
	FString DescribeSections(const FMantleGlbMesh& Mesh)
	{
		FString Out;
		for (const FMantleGlbPrimitive& Primitive : Mesh.Primitives)
		{
			Out += FString::Printf(TEXT("%s%s %dv/%dt"), Out.IsEmpty() ? TEXT("") : TEXT(", "),
				Mesh.MaterialNames.IsValidIndex(Primitive.MaterialIndex) ? *Mesh.MaterialNames[Primitive.MaterialIndex] : TEXT("?"), Primitive.NumVertices, Primitive.Indices.Num() / 3);
		}
		return Out;
	}

	FString DescribeSections(const FMantleFaceLayout& Layout)
	{
		FString Out;
		for (const FMantleFaceLayoutSection& Section : Layout.Sections)
		{
			Out += FString::Printf(TEXT("%s%s %dv/%dt"), Out.IsEmpty() ? TEXT("") : TEXT(", "), *Section.Material, Section.NumVertices, Section.NumTriangles);
		}
		return Out;
	}

	template <typename T>
	void ReorderArray(TArray<T>& Array, const TArray<int32>& InputOfReference, const int32 Stride)
	{
		const int32 N = InputOfReference.Num();
		if (Array.Num() != N * Stride)
		{
			return;
		}
		TArray<T> Out;
		Out.SetNumUninitialized(Array.Num());
		for (int32 Reference = 0; Reference < N; ++Reference)
		{
			for (int32 Slot = 0; Slot < Stride; ++Slot)
			{
				Out[Reference * Stride + Slot] = Array[InputOfReference[Reference] * Stride + Slot];
			}
		}
		Array = MoveTemp(Out);
	}
}

FString FMantleFaceLayout::DefaultPath()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("MantleCore"));
	return Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("FaceLayout"), TEXT("face-layout-v1.json")) : FString();
}

const FMantleFaceLayout* FMantleFaceLayout::Get()
{
	static TUniquePtr<FMantleFaceLayout> Loaded;
	static bool bTried = false;
	if (!bTried)
	{
		bTried = true;
		const FString Path = DefaultPath();
		FString Json, Error;
		FMantleFaceLayout Layout;
		if (Path.IsEmpty() || !FFileHelper::LoadFileToString(Json, *Path))
		{
			UE_LOG(LogMantleFaceLayout, Display, TEXT("MANTLE_FACE_LAYOUT no layout file at %s"), *Path);
		}
		else if (!Parse(Json, Layout, Error))
		{
			UE_LOG(LogMantleFaceLayout, Warning, TEXT("MANTLE_FACE_LAYOUT %s is not usable: %s"), *Path, *Error);
		}
		else
		{
			Loaded = MakeUnique<FMantleFaceLayout>(MoveTemp(Layout));
		}
	}
	return Loaded.Get();
}

int32 FMantleFaceLayout::FindFaceLOD(const USkeletalMesh* Face)
{
	const FSkeletalMeshRenderData* RenderData = Face ? Face->GetResourceForRendering() : nullptr;
	if (!RenderData)
	{
		return INDEX_NONE;
	}
	for (int32 LOD = 0; LOD < RenderData->LODRenderData.Num(); ++LOD)
	{
		if (RenderData->LODRenderData[LOD].GetTotalFaces() == ReferenceTriangles)
		{
			return LOD;
		}
	}
	return INDEX_NONE;
}

bool FMantleFaceLayout::ReadRender(const USkeletalMesh* Face, const int32 LODIndex, FMantleGlbMesh& Out, FString& OutError)
{
	using namespace MantleFaceLayoutPrivate;
	Out = FMantleGlbMesh();
	const FSkeletalMeshRenderData* RenderData = Face ? Face->GetResourceForRendering() : nullptr;
	if (!RenderData || !RenderData->LODRenderData.IsValidIndex(LODIndex))
	{
		return Fail(OutError, TEXT("the face has no render data for that LOD"));
	}
	const FSkeletalMeshLODRenderData& LOD = RenderData->LODRenderData[LODIndex];
	const FPositionVertexBuffer& Positions = LOD.StaticVertexBuffers.PositionVertexBuffer;
	const FStaticMeshVertexBuffer& Vertices = LOD.StaticVertexBuffers.StaticMeshVertexBuffer;
	const int32 N = static_cast<int32>(Positions.GetNumVertices());
	if (N == 0 || !Positions.GetVertexData() || Vertices.GetNumVertices() != static_cast<uint32>(N) || Vertices.GetNumTexCoords() == 0
		|| !Vertices.GetTexCoordData() || !LOD.MultiSizeIndexContainer.IsIndexBufferValid())
	{
		return Fail(OutError, TEXT("the face has no CPU positions, UVs or indices"));
	}
	TArray<uint32> Indices;
	LOD.MultiSizeIndexContainer.GetIndexBuffer(Indices);
	Out.Positions.SetNumUninitialized(N);
	Out.UVs.SetNumUninitialized(N);
	for (int32 Vertex = 0; Vertex < N; ++Vertex)
	{
		Out.Positions[Vertex] = Positions.VertexPosition(Vertex);
		Out.UVs[Vertex] = Vertices.GetVertexUV(Vertex, 0);
	}
	int32 Covered = 0;
	for (int32 Section = 0; Section < LOD.RenderSections.Num(); ++Section)
	{
		const FSkelMeshRenderSection& Render = LOD.RenderSections[Section];
		if (static_cast<int32>(Render.BaseVertexIndex) != Covered)
		{
			return Fail(OutError, TEXT("the face's render sections do not tile its vertices in order"));
		}
		Covered += static_cast<int32>(Render.NumVertices);
		const TArray<FSkeletalMaterial>& Materials = Face->GetMaterials();
		FString Material = Materials.IsValidIndex(Render.MaterialIndex) ? Materials[Render.MaterialIndex].MaterialSlotName.ToString() : FString::Printf(TEXT("section_%d"), Section);
		if (Out.MaterialNames.Contains(Material))
		{
			Material = FString::Printf(TEXT("%s_%d"), *Material, Section);
		}
		Out.MaterialNames.Add(Material);
		FMantleGlbPrimitive& Primitive = Out.Primitives.AddDefaulted_GetRef();
		Primitive.MaterialIndex = Out.MaterialNames.Num() - 1;
		Primitive.FirstVertex = static_cast<int32>(Render.BaseVertexIndex);
		Primitive.NumVertices = static_cast<int32>(Render.NumVertices);
		const int32 Begin = static_cast<int32>(Render.BaseIndex);
		const int32 End = Begin + static_cast<int32>(Render.NumTriangles) * 3;
		if (End > Indices.Num())
		{
			return Fail(OutError, TEXT("a face section runs past its index buffer"));
		}
		for (int32 Index = Begin; Index < End; ++Index)
		{
			const int64 Local = static_cast<int64>(Indices[Index]) - Render.BaseVertexIndex;
			if (Local < 0 || Local >= Render.NumVertices)
			{
				return Fail(OutError, TEXT("a face triangle uses another section's vertices"));
			}
			Primitive.Indices.Add(static_cast<uint32>(Local));
		}
	}
	if (Covered != N)
	{
		return Fail(OutError, TEXT("the face has vertices outside its sections"));
	}
	return true;
}

bool FMantleFaceLayout::Record(const USkeletalMesh* Face, FMantleFaceLayout& Out, FString& OutError)
{
	Out = FMantleFaceLayout();
	const int32 LOD = FindFaceLOD(Face);
	if (LOD == INDEX_NONE)
	{
		OutError = FString::Printf(TEXT("the face has no LOD with %d triangles"), ReferenceTriangles);
		return false;
	}
	FMantleGlbMesh Mesh;
	if (!ReadRender(Face, LOD, Mesh, OutError))
	{
		return false;
	}
	for (const FVector2f& UV : Mesh.UVs)
	{
		Out.QuantizedUVs.Add(FIntPoint(FMath::RoundToInt32(UV.X * Quantization), FMath::RoundToInt32(UV.Y * Quantization)));
	}
	for (const FMantleGlbPrimitive& Primitive : Mesh.Primitives)
	{
		FMantleFaceLayoutSection& Section = Out.Sections.AddDefaulted_GetRef();
		Section.Material = Mesh.MaterialNames[Primitive.MaterialIndex];
		Section.FirstVertex = Primitive.FirstVertex;
		Section.NumVertices = Primitive.NumVertices;
		Section.FirstIndex = Out.Indices.Num();
		Section.NumTriangles = Primitive.Indices.Num() / 3;
		for (const uint32 Index : Primitive.Indices)
		{
			Out.Indices.Add(Index + static_cast<uint32>(Primitive.FirstVertex));
		}
	}
	return true;
}

bool FMantleFaceLayout::Reorder(FMantleGlbMesh& Mesh, FString& OutNote, FString& OutError) const
{
	using namespace MantleFaceLayoutPrivate;
	OutNote.Reset();
	const int32 N = QuantizedUVs.Num();
	if (N == 0 || Mesh.Positions.Num() != N || Mesh.UVs.Num() != N)
	{
		return Fail(OutError, FString::Printf(TEXT("the face has %d vertices and the built face layout %d (face sections: %s; layout sections: %s)"), Mesh.Positions.Num(), N,
			*DescribeSections(Mesh), *DescribeSections(*this)));
	}
	if (Mesh.Primitives.Num() != Sections.Num())
	{
		return Fail(OutError, FString::Printf(TEXT("the face has %d sections and the built face layout %d (face: %s; layout: %s)"), Mesh.Primitives.Num(), Sections.Num(),
			*DescribeSections(Mesh), *DescribeSections(*this)));
	}
	// Pair each layout section with a face section of the same size (the material name breaks a tie).
	TArray<int32> PrimitiveOfSection;
	PrimitiveOfSection.Init(INDEX_NONE, Sections.Num());
	TBitArray<> Used(false, Mesh.Primitives.Num());
	for (int32 Section = 0; Section < Sections.Num(); ++Section)
	{
		int32 Chosen = INDEX_NONE;
		for (int32 Primitive = 0; Primitive < Mesh.Primitives.Num(); ++Primitive)
		{
			const FMantleGlbPrimitive& Candidate = Mesh.Primitives[Primitive];
			if (Used[Primitive] || Candidate.NumVertices != Sections[Section].NumVertices || Candidate.Indices.Num() != Sections[Section].NumTriangles * 3) { continue; }
			const bool bSameName = Mesh.MaterialNames.IsValidIndex(Candidate.MaterialIndex) && Mesh.MaterialNames[Candidate.MaterialIndex].Equals(Sections[Section].Material, ESearchCase::IgnoreCase);
			if (Chosen == INDEX_NONE || bSameName) { Chosen = Primitive; }
			if (bSameName) { break; }
		}
		if (Chosen == INDEX_NONE)
		{
			return Fail(OutError, FString::Printf(TEXT("no face section matches the layout's section %s (%d vertices, %d triangles); face: %s; layout: %s"), *Sections[Section].Material,
				Sections[Section].NumVertices, Sections[Section].NumTriangles, *DescribeSections(Mesh), *DescribeSections(*this)));
		}
		Used[Chosen] = true;
		PrimitiveOfSection[Section] = Chosen;
	}
	// Inside each section: the vertices by UV (and neighbours), the triangles but for flipped diagonals.
	TArray<int32> InputOfReference;
	InputOfReference.Init(INDEX_NONE, N);
	int32 Exact = 0, FlippedSections = 0, FlippedPairs = 0;
	for (int32 Section = 0; Section < Sections.Num(); ++Section)
	{
		const FMantleFaceLayoutSection& Layout = Sections[Section];
		const FMantleGlbPrimitive& Primitive = Mesh.Primitives[PrimitiveOfSection[Section]];
		FMantleBodyCorrespondence Reference;
		for (int32 Vertex = 0; Vertex < Layout.NumVertices; ++Vertex) { Reference.QuantizedUVs.Add(QuantizedUVs[Layout.FirstVertex + Vertex]); }
		for (int32 Index = 0; Index < Layout.NumTriangles * 3; ++Index) { Reference.Indices.Add(Indices[Layout.FirstIndex + Index] - static_cast<uint32>(Layout.FirstVertex)); }
		const TConstArrayView<FVector2f> UVs(Mesh.UVs.GetData() + Primitive.FirstVertex, Primitive.NumVertices);
		TArray<int32> Local;
		FString Error;
		if (Reference.Match(UVs, Primitive.Indices, Local, Error))
		{
			++Exact;
		}
		else
		{
			FString Note;
			int32 Pairs = 0;
			FString FlipError;
			if (!Reference.MatchFlippedDiagonals(UVs, Primitive.Indices, Local, Note, FlipError, MAX_int32, &Pairs))
			{
				return Fail(OutError, FString::Printf(TEXT("section %s: exact match: %s; flipped-diagonal match: %s"), *Layout.Material, *Error, *FlipError));
			}
			++FlippedSections;
			FlippedPairs += Pairs;
		}
		for (int32 Vertex = 0; Vertex < Layout.NumVertices; ++Vertex)
		{
			InputOfReference[Layout.FirstVertex + Vertex] = Primitive.FirstVertex + Local[Vertex];
		}
	}
	// Everything checked: now change the mesh.
	ReorderArray(Mesh.Positions, InputOfReference, 1);
	ReorderArray(Mesh.Normals, InputOfReference, 1);
	ReorderArray(Mesh.Tangents, InputOfReference, 1);
	ReorderArray(Mesh.UVs, InputOfReference, 1);
	ReorderArray(Mesh.UV1s, InputOfReference, 1);
	ReorderArray(Mesh.Colors, InputOfReference, 1);
	ReorderArray(Mesh.HairRootUV, InputOfReference, 1);
	ReorderArray(Mesh.HairCoordU, InputOfReference, 1);
	if (Mesh.NumInfluences > 0)
	{
		ReorderArray(Mesh.InfluenceJoints, InputOfReference, Mesh.NumInfluences);
		ReorderArray(Mesh.InfluenceWeights, InputOfReference, Mesh.NumInfluences);
	}
	TArray<FMantleGlbPrimitive> Rebuilt;
	for (int32 Section = 0; Section < Sections.Num(); ++Section)
	{
		const FMantleFaceLayoutSection& Layout = Sections[Section];
		FMantleGlbPrimitive& Primitive = Rebuilt.AddDefaulted_GetRef();
		Primitive.MaterialIndex = Mesh.Primitives[PrimitiveOfSection[Section]].MaterialIndex;
		Primitive.FirstVertex = Layout.FirstVertex;
		Primitive.NumVertices = Layout.NumVertices;
		for (int32 Index = 0; Index < Layout.NumTriangles * 3; ++Index) { Primitive.Indices.Add(Indices[Layout.FirstIndex + Index] - static_cast<uint32>(Layout.FirstVertex)); }
	}
	Mesh.Primitives = MoveTemp(Rebuilt);
	OutNote = FString::Printf(TEXT("face put in the built face layout (%s): %d sections, %d matched exactly, %d with %d flipped quad diagonals"), *Source, Sections.Num(), Exact, FlippedSections, FlippedPairs);
	OutError.Reset();
	return true;
}

bool FMantleFaceLayout::Parse(const FString& Json, FMantleFaceLayout& Out, FString& OutError)
{
	Out = FMantleFaceLayout();
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{
		OutError = TEXT("not valid JSON");
		return false;
	}
	int32 Version = 0, Count = 0, Triangles = 0;
	const TArray<TSharedPtr<FJsonValue>>* UVs = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* IndexValues = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* SectionValues = nullptr;
	if (!Root->TryGetNumberField(TEXT("version"), Version) || Version != 1 || !Root->TryGetNumberField(TEXT("vertexCount"), Count) || Count <= 0
		|| !Root->TryGetNumberField(TEXT("triangleCount"), Triangles) || Triangles <= 0 || !Root->TryGetArrayField(TEXT("quantizedUVs"), UVs)
		|| !Root->TryGetArrayField(TEXT("indices"), IndexValues) || !Root->TryGetArrayField(TEXT("sections"), SectionValues)
		|| UVs->Num() != Count || IndexValues->Num() != Triangles * 3)
	{
		OutError = TEXT("not a version 1 face layout with matching counts");
		return false;
	}
	Root->TryGetStringField(TEXT("source"), Out.Source);
	Root->TryGetStringField(TEXT("generated"), Out.Generated);
	for (const TSharedPtr<FJsonValue>& Value : *UVs)
	{
		const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
		if (!Value.IsValid() || !Value->TryGetArray(Pair) || Pair->Num() != 2)
		{
			OutError = TEXT("a quantised UV is not a pair");
			return false;
		}
		Out.QuantizedUVs.Add(FIntPoint(static_cast<int32>((*Pair)[0]->AsNumber()), static_cast<int32>((*Pair)[1]->AsNumber())));
	}
	for (const TSharedPtr<FJsonValue>& Value : *IndexValues)
	{
		double Number = 0.0;
		if (!Value.IsValid() || !Value->TryGetNumber(Number) || Number < 0.0 || Number >= Count)
		{
			OutError = TEXT("a triangle index is out of range");
			return false;
		}
		Out.Indices.Add(static_cast<uint32>(Number));
	}
	int32 NextVertex = 0, NextIndex = 0;
	for (const TSharedPtr<FJsonValue>& Value : *SectionValues)
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		FMantleFaceLayoutSection Section;
		if (!Object.IsValid() || !Object->TryGetStringField(TEXT("material"), Section.Material) || !Object->TryGetNumberField(TEXT("firstVertex"), Section.FirstVertex)
			|| !Object->TryGetNumberField(TEXT("numVertices"), Section.NumVertices) || !Object->TryGetNumberField(TEXT("firstIndex"), Section.FirstIndex)
			|| !Object->TryGetNumberField(TEXT("numTriangles"), Section.NumTriangles) || Section.FirstVertex != NextVertex || Section.FirstIndex != NextIndex
			|| Section.NumVertices <= 0 || Section.NumTriangles <= 0)
		{
			OutError = TEXT("a section is missing a field or does not follow the one before");
			return false;
		}
		NextVertex += Section.NumVertices;
		NextIndex += Section.NumTriangles * 3;
		Out.Sections.Add(MoveTemp(Section));
	}
	if (NextVertex != Count || NextIndex != Out.Indices.Num())
	{
		OutError = TEXT("the sections do not cover the vertices and triangles");
		return false;
	}
	return true;
}

FString FMantleFaceLayout::ToJson() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetStringField(TEXT("comment"), TEXT("Maintainer-recorded by Mantle.Dev.RecordFaceLayout. Plugin-only; not part of the synced avatar contract. The layout of a built MetaHuman face (Epic's UE Optimized / Medium LOD0): quantised UVs (x2048) and triangles in built order, and the render sections. No geometry."));
	Root->SetStringField(TEXT("source"), Source);
	Root->SetStringField(TEXT("generated"), Generated);
	Root->SetNumberField(TEXT("vertexCount"), QuantizedUVs.Num());
	Root->SetNumberField(TEXT("triangleCount"), Indices.Num() / 3);
	Root->SetNumberField(TEXT("quantization"), Quantization);
	TArray<TSharedPtr<FJsonValue>> UVs;
	for (const FIntPoint& UV : QuantizedUVs)
	{
		TArray<TSharedPtr<FJsonValue>> Pair;
		Pair.Add(MakeShared<FJsonValueNumber>(UV.X));
		Pair.Add(MakeShared<FJsonValueNumber>(UV.Y));
		UVs.Add(MakeShared<FJsonValueArray>(Pair));
	}
	Root->SetArrayField(TEXT("quantizedUVs"), UVs);
	TArray<TSharedPtr<FJsonValue>> IndexValues;
	for (const uint32 Index : Indices) { IndexValues.Add(MakeShared<FJsonValueNumber>(Index)); }
	Root->SetArrayField(TEXT("indices"), IndexValues);
	TArray<TSharedPtr<FJsonValue>> SectionValues;
	for (const FMantleFaceLayoutSection& Section : Sections)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("material"), Section.Material);
		Object->SetNumberField(TEXT("firstVertex"), Section.FirstVertex);
		Object->SetNumberField(TEXT("numVertices"), Section.NumVertices);
		Object->SetNumberField(TEXT("firstIndex"), Section.FirstIndex);
		Object->SetNumberField(TEXT("numTriangles"), Section.NumTriangles);
		SectionValues.Add(MakeShared<FJsonValueObject>(Object));
	}
	Root->SetArrayField(TEXT("sections"), SectionValues);
	FString Out;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}
