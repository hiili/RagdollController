// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <array>
#include <cstddef>

#include "ObjectSelector.generated.h"








/* selector struct members */




UENUM( BlueprintType, Category = "Object Selector" )
enum class EObjectSelectorMobilityFilter : uint8
{
	Any,
	Static,
	Stationary,
	Movable,
	ENUM_LAST UMETA(Hidden)
};


USTRUCT( BlueprintType, Category = "Object Selector" )
struct FObjectSelectorFilter
{
	GENERATED_BODY()


	/** Include only objects that have this tag. */
	UPROPERTY( EditAnywhere )
	FName NarrowByTag;

	/** Include only objects that are of this or a derived type. */
	UPROPERTY( EditAnywhere )
	UClass * NarrowByClass = 0;

	/** Include only objects whose mobility matches this. */
	UPROPERTY( EditAnywhere )
	EObjectSelectorMobilityFilter NarrowByMobility = EObjectSelectorMobilityFilter::Any;

	/** Include only objects whose name matches this pattern (? = any character, * = any sequence). */
	UPROPERTY( EditAnywhere )
	FString NarrowByNamePattern;

	/** Include only objects that have a tag that matches this pattern (? = any character, * = any sequence). */
	UPROPERTY( EditAnywhere )
	FString NarrowByTagPattern;

};








/* selector structs */




/**
 * 
 */
USTRUCT( BlueprintType, Category = "Object Selector" )
struct FComponentSelector
{
	/* 
	 * We use an USTRUCT and a separate Blueprint helper UCLASS instead of making this directly an UCLASS. This is because compositing an UCLASS inside another
	 * class must be done via a pointer, which in turn causes the struct/class to show up in the editor in a less intuitive and usable manner. (as of UE 4.9)
	 */

	GENERATED_BODY()

	/** Include all objects that have their name listed here. */
	UPROPERTY( EditAnywhere )
	TArray<FName> IncludeByName;

	/** Include all objects that have any of these tags. */
	UPROPERTY( EditAnywhere )
	TArray<FName> IncludeByTag;

	/** Include all objects that match any of these filters. */
	UPROPERTY( EditAnywhere )
	TArray<FObjectSelectorFilter> IncludeByFilter;
	

	/* C++ interface; see blueprint helper section below for the Blueprint interface */

	/** Test whether a component matches the selection criteria. */
	FORCEINLINE bool IsMatching( const UActorComponent & component ) const;

	/** Filter an array by removing all elements that do not match the selection criteria. The array order of the objects left is not preserved. A reference
	 *  to the same array is returned. */
	template<typename T>
	TArray<T *> & FilterArray( TArray<T *> & array ) const;

	/** Returns an array containing all matching components in the provided actor. */
	template<typename T = UActorComponent>
	TArray<T *> GetAllMatchingComponents( const AActor & actor ) const;

	/** Returns an array containing all matching components in the provided world, regardless of the owning actor. */
	template<typename T = UActorComponent>
	TArray<T *> GetAllMatchingComponents( UWorld & world ) const;


protected:

	template<typename this_t, typename T>
	TArray<T *> & FilterArrayImpl( TArray<T *> & array ) const;

	/** A mapping from our mobility type to UE mobility type (our mobility enum cannot match UE's because we want the 'Any' mobility to be the first one, so
	 ** that it becomes the default in the editor. we could compute an offset and static_assert some things, but this is more robust and no more complex) */
	static const std::array<EComponentMobility::Type, 4> OurMobilityToUEMobilityMap;

private:
	static std::array<EComponentMobility::Type, 4> InitializeMobilityMap();

};


/**
 * 
 */
USTRUCT( BlueprintType, Category = "Object Selector" )
struct FActorSelector : public FComponentSelector
{
	GENERATED_BODY()

	/** Include all actors that are referenced here. */
	UPROPERTY( EditAnywhere )
	TArray<AActor *> IncludeByReference;


	/** Test whether an actor matches the selection criteria. */
	FORCEINLINE bool IsMatching( const AActor & actor ) const;   // we hide IsMatching( const UActorComponent * ) from the base class

	/** Filter an array by removing all elements that do not match the selection criteria. The array order of the objects left is not preserved. A reference
	 *  to the same array is returned. */
	template<typename T>
	TArray<T *> & FilterArray( TArray<T *> & array ) const;

	/** Returns an array containing all matching actors in the provided world. */
	template<typename T = AActor>
	TArray<T *> GetAllMatchingActors( UWorld & world ) const;
};








/* blueprint helpers */




UCLASS()
class UComponentSelectorBlueprintHelpers : public UObject
{
	GENERATED_BODY()

public:

	/** Test whether a component matches the selection criteria. */
	UFUNCTION( BlueprintCallable, BlueprintPure, Category = "Object Selector" )
	static bool IsMatchingComponent( const FComponentSelector & ComponentSelector, const UActorComponent * Component );

	/** Filter an array by removing all elements that do not match the selection criteria. The array order of the objects left is not preserved. */
	UFUNCTION( BlueprintCallable, Category = "Object Selector" )
	static void FilterComponentArray( const FComponentSelector & ComponentSelector, UPARAM( ref ) TArray<UActorComponent *> & Array );

	/** Returns an array containing all matching components in the world, regardless of the owning actor.
	 *  @param Class Narrow the search to include only objects of this type. Also defines the type of the output array.
	 *         Must be specified or the result array will be empty. */
	UFUNCTION( BlueprintCallable, Category = "Object Selector", meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "Class", DynamicOutputParam = "Out") )
	static void GetAllMatchingComponentsInWorld( const FComponentSelector & ComponentSelector, const UObject * WorldContextObject, TSubclassOf<UActorComponent> Class, TArray<UActorComponent *> & Out );

	/** Returns an array containing all matching components in the provided actor.
	 *  @param Class Narrow the search to include only objects of this type. Also defines the type of the output array.
	 *         Must be specified or the result array will be empty. */
	UFUNCTION( BlueprintCallable, Category = "Object Selector", meta = (DeterminesOutputType = "Class", DynamicOutputParam = "Out") )
	static void GetAllMatchingComponentsInActor( const FComponentSelector & ComponentSelector, const AActor * actor, TSubclassOf<UActorComponent> Class, TArray<UActorComponent *> & Out );
};


UCLASS()
class UActorSelectorBlueprintHelpers : public UObject
{
	GENERATED_BODY()

public:

	/** Test whether an actor matches the selection criteria. */
	UFUNCTION( BlueprintCallable, BlueprintPure, Category = "Object Selector" )
	static bool IsMatchingActor( const FActorSelector & ActorSelector, const AActor * Actor );

	/** Filter an array by removing all elements that do not match the selection criteria. The array order of the objects left is not preserved. */
	UFUNCTION( BlueprintCallable, Category = "Object Selector" )
	static void FilterActorArray( const FActorSelector & ActorSelector, UPARAM( ref ) TArray<AActor *> & Array );

	/** Returns an array containing all matching actors in the world.
	 *  @param Class Narrow the search to include only objects of this type. Also defines the type of the output array.
	 *         Must be specified or the result array will be empty. */
	UFUNCTION( BlueprintCallable, Category = "Object Selector", meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "Class", DynamicOutputParam = "Out") )
	static void GetAllMatchingActors( const FActorSelector & ActorSelector, const UObject * WorldContextObject, TSubclassOf<AActor> Class, TArray<AActor *> & Out );
};








/* implementations for templated functions */




bool FComponentSelector::IsMatching( const UActorComponent & component ) const
{
	UE_LOG( LogTemp, Error, TEXT( "Component IsMatching: %s" ), *component.GetName() );

	// IncludeByName
	if( IncludeByName.Contains( component.GetFName() ) ) return true;

	// IncludeByTag
	for( const FName & tag : IncludeByTag )
	{
		if( component.ComponentHasTag( tag ) ) return true;
	}

	// IncludeByFilter:
	//   - for each filter
	//     - no match: continue with next filter
	//     - have match: return true (filters combine in an OR fashion, so we require only one filter to match)
	for( const FObjectSelectorFilter & filter : IncludeByFilter )
	{
		// NarrowByTag
		if( !filter.NarrowByTag.IsNone() && !component.ComponentHasTag( filter.NarrowByTag ) ) continue;

		// NarrowByClass
		if( filter.NarrowByClass && !component.IsA( filter.NarrowByClass ) ) continue;

		// NarrowByMobility: reject if not a USceneComponent (thus no mobility) or has a different mobility than required here
		if( filter.NarrowByMobility != EObjectSelectorMobilityFilter::Any )
		{
			const USceneComponent * sceneComponent = dynamic_cast<const USceneComponent *>(&component);
			if( !sceneComponent ||
				OurMobilityToUEMobilityMap[static_cast<std::size_t>(filter.NarrowByMobility)] != sceneComponent->Mobility ) continue;
		}

		// NarrowByNamePattern
		if( !filter.NarrowByNamePattern.IsEmpty() && !FWildcardString( filter.NarrowByNamePattern ).IsMatch( component.GetName() ) ) continue;

		// NarrowByTagPattern
		if( !filter.NarrowByTagPattern.IsEmpty() )
		{
			bool found = false;
			for( const FName & tag : component.ComponentTags )
			{
				if( FWildcardString( filter.NarrowByTagPattern ).IsMatch( tag.ToString() ) )
				{
					found = true;
					break;
				}
			}

			// we have a tag pattern but no matches: have to keep trying with following filters
			if( !found ) continue;
		}

		// all narrowing checks passed: this filter has a match
		return true;
	}

	// no explicit matches, and none of the filters matched: no match
	return false;
}


bool FActorSelector::IsMatching( const AActor & actor ) const
{
	UE_LOG( LogTemp, Error, TEXT( "Actor IsMatching: %s" ), *actor.GetName() );
	return false;
}




template<typename T>
TArray<T *> & FComponentSelector::FilterArray( TArray<T *> & array ) const
{
	return FilterArrayImpl<FComponentSelector>( array );
}


template<typename T>
TArray<T *> & FActorSelector::FilterArray( TArray<T *> & array ) const
{
	return FilterArrayImpl<FActorSelector>( array );
}


template<typename this_t, typename T>
TArray<T *> & FComponentSelector::FilterArrayImpl( TArray<T *> & array ) const
{
	// iterate over the array
	for( int32 i = 0; i < array.Num(); ++i )
	{
		// remove if null or not matching
		if( !array[i] || !static_cast<const this_t * const>(this)->IsMatching( *array[i] ) )
		{
			// remove (don't preserve order, don't shrink yet)
			array.RemoveAtSwap( i, 1, false );

			// have this index position re-checked
			--i;
		}
	}

	// shrink array
	array.Shrink();

	return array;
}



template<typename T /*= UActorComponent*/>
TArray<T *>
FComponentSelector::GetAllMatchingComponents( const AActor & actor ) const
{
	TArray<T *> result;
	actor.GetComponents( result );
	return FilterArray( result );
}




template<typename T /*= UActorComponent*/>
TArray<T *>
FComponentSelector::GetAllMatchingComponents( UWorld & world ) const
{
	TArray<T *> result;

	// We could just use GetObjectsOfClass and filter the result, but we would have to be careful to filter the components properly to avoid components from
	// the editor world, from hidden levels, etc. Play safe and avoid possible inconsistencies: do this by enumerating actors via the standard actor iterator
	for( TActorIterator<AActor> it( &world ); it; ++it )
	{
		result.Append( GetAllMatchingComponents( **it ) );
	}

	return result;
}




template<typename T /*= AActor*/>
TArray<T *>
FActorSelector::GetAllMatchingActors( UWorld & world ) const
{
	TArray<T *> result;

	for( TActorIterator<AActor> it( &world ); it; ++it )
	{
		if( IsMatching( **it ) )
		{
			result.Add( *it );
		}
	}

	return result;
}
