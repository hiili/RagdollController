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


/** A set of filter conditions for matching against AActor and UActorComponent objects. */
USTRUCT()
struct FObjectSelectorFilter
{
	GENERATED_BODY()

	/** Include only objects that have this tag (not case sensitive, fast; involves only an index comparison). */
	UPROPERTY( EditAnywhere )
	FName NarrowByTag;

	/** Include only objects whose mobility matches this. */
	UPROPERTY( EditAnywhere )
	EObjectSelectorMobilityFilter NarrowByMobility = EObjectSelectorMobilityFilter::Any;

	/** Include only objects whose name matches this ?*-pattern (is case sensitive, slow; involves full string matching). */
	UPROPERTY( EditAnywhere )
	FString NarrowByNamePattern;

	/** Include only objects that have a tag that matches this ?*-pattern (is case sensitive, slow; involves full string matching). */
	UPROPERTY( EditAnywhere )
	FString NarrowByTagPattern;
};


/** A set of filter conditions for matching against UActorComponent objects. This is not intended to be used directly; use FActorSelector instead! */
USTRUCT()
struct FComponentSelectorFilter : public FObjectSelectorFilter
{
	GENERATED_BODY()

	/** Include only components that are derived from this type. */
	UPROPERTY( EditAnywhere )
	TSubclassOf<UActorComponent> NarrowByClass;
};


/** A set of filter conditions for matching against AActor objects. This is not intended to be used directly; use FActorSelector instead! */
USTRUCT()
struct FActorSelectorFilter : public FObjectSelectorFilter
{
	GENERATED_BODY()

	/** Include only actors that are derived from this type. */
	UPROPERTY( EditAnywhere )
	TSubclassOf<AActor> NarrowByClass;
};








/* selector structs */




/**
* A common base type for the component and actor selector types. Treat this as an abstract base class.
* 
* We use an USTRUCT and a separate Blueprint helper UCLASS instead of making this directly an UCLASS. This is because compositing an UCLASS inside another
* class must be done via a pointer, which in turn causes the struct/class to show up in the editor in a less intuitive and usable manner. (as of UE 4.9)
*
* This could be re-factored into a relatively generic UObject selector if the need arises (with some method-matching code for tags).
*/
USTRUCT()
struct FObjectSelector
{
	GENERATED_BODY()

	/** Include all objects that have their name listed here (not case sensitive, fast; involves only an index comparison).
	 *  
	 *  Note that inclusion by this field requires an exact match and the engine tends to internally add an underscore-delimited suffix to most objects!
	 *  Such suffixing can be avoided sometimes by using object names that do not collide with known class names.
	 *  If you are not sure whether your object's name will get suffixed, you might want to use wildcard matching, which is available under Include By Filter. */
	UPROPERTY( EditAnywhere )
	TArray<FName> IncludeByName;

	/** Include all objects that have any of these tags (not case sensitive, fast; involves only an index comparison). */
	UPROPERTY( EditAnywhere )
	TArray<FName> IncludeByTag;


protected:

	/* Treat this as an abstract base class (UE needs to be able construct a CDO, though) */
	//FObjectSelector() {}


	/** static polymorphic downcast helper */
	template<typename this_t>
	this_t * statically_polymorphic_this()   { return static_cast<this_t *>(this); }

	/** static polymorphic downcast helper (const) */
	template<typename this_t>
	const this_t * statically_polymorphic_this() const   { return static_cast<const this_t *>(this); }


	/** Common IsMatching() implementation.
	 *  Use a statically polymorphic template method pattern (we cannot do class-level CRTP due to Unreal Header Tool limitations). */
	template<typename this_t, typename object_t>
	bool IsMatching( const object_t & object ) const;

	/** Common FilterArray() implementation. (see comments of IsMatching()) */
	template<typename this_t, typename T>
	TArray<T *> & FilterArray( TArray<T *> & array ) const;


	/** A mapping from our mobility type to UE mobility type (our mobility enum cannot match UE's because we want the 'Any' mobility to be the first one, so
	 *  that it becomes the default in the editor. we could compute an offset and static_assert some things, but this is more robust and no more complex) */
	static const std::array<EComponentMobility::Type, 4> OurMobilityToUEMobilityMap;

	static std::array<EComponentMobility::Type, 4> InitializeOurMobilityToUEMobilityMap();


private:

	/* object accessors */

	FORCEINLINE bool HasTag( const UActorComponent & component, const FName & tag ) const;
	FORCEINLINE bool HasTag( const AActor & actor, const FName & tag ) const;
	
	FORCEINLINE const TArray<FName> & GetTags( const UActorComponent & component ) const;
	FORCEINLINE const TArray<FName> & GetTags( const AActor & actor ) const;

	/** Return the component's mobility if it is a USceneComponent. Otherwise return 0xff to signal the lack of mobility data. */
	FORCEINLINE EComponentMobility::Type GetMobility( const UActorComponent & component ) const;

	/** Return the mobility of the actor's root component if one exists. Otherwise return 0xff to signal the lack of mobility data. */
	FORCEINLINE EComponentMobility::Type GetMobility( const AActor & actor ) const;

};


/** Class for selecting a set of actor components based on various selection criteria. */
USTRUCT( BlueprintType, Category = "Object Selector" )
struct FComponentSelector : public FObjectSelector
{
	GENERATED_BODY()

	/** Include all objects that match any of these filters. */
	UPROPERTY( EditAnywhere )
	TArray<FComponentSelectorFilter> IncludeByFilter;
	

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

	/** Returns an array containing all matching components in the provided world, regardless of the owning actor.
	 *  Only actors derived form ownerActorClass are included in the actual search; this can provide a considerable speedup. */
	template<typename T = UActorComponent>
	TArray<T *> GetAllMatchingComponents( UWorld & world, UClass * ownerActorClass = AActor::StaticClass() ) const;

};


/** Class for selecting a set of actors based on various selection criteria. */
USTRUCT( BlueprintType, Category = "Object Selector" )
struct FActorSelector : public FObjectSelector
{
	GENERATED_BODY()

	/** Include all objects that match any of these filters. */
	UPROPERTY( EditAnywhere )
	TArray<FActorSelectorFilter> IncludeByFilter;

	/** Include all actors that are directly referenced here. */
	UPROPERTY( EditAnywhere )
	TArray<AActor *> IncludeByReference;


	/* C++ interface; see blueprint helper section below for the Blueprint interface */

	/** Test whether an actor matches the selection criteria. */
	FORCEINLINE bool IsMatching( const AActor & actor ) const;

	/** Filter an array by removing all elements that do not match the selection criteria. The array order of the objects left is not preserved. A reference
	 *  to the same array is returned. */
	template<typename T>
	TArray<T *> & FilterArray( TArray<T *> & array ) const;

	/** Returns an array containing all matching actors in the provided world. Only actors derived form uClass are included in the actual search; this can
	 * provide a considerable speedup. */
	template<typename T = AActor>
	TArray<T *> GetAllMatchingActors( UWorld & world, UClass * uClass = AActor::StaticClass() ) const;
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

	/** Filter an array by removing all elements that do not match the selection criteria. The array order of the remaining objects is not preserved. */
	UFUNCTION( BlueprintCallable, Category = "Object Selector" )
	static void FilterComponentArray( const FComponentSelector & ComponentSelector, UPARAM( ref ) TArray<UActorComponent *> & Array );

	/** Returns an array containing all matching components in the provided actor.
	 *  @param OutType   Defines the type of the output array. Note that this does not narrow the search;
	 *                   make sure that the found components actually fit this type!
	 *                   Can be empty, in which case the output array becomes an Actor Component array. */
	UFUNCTION( BlueprintCallable, Category = "Object Selector", meta = (DeterminesOutputType = "OutType", DynamicOutputParam = "Out") )
	static void GetAllMatchingComponentsInActor( const FComponentSelector & ComponentSelector, const AActor * actor,
		TSubclassOf<UActorComponent> OutType, TArray<UActorComponent *> & Out );

	/** Returns an array containing all matching components in the world, regardless of the owning actor.
	 *  @param OwnerActorClass   Narrow the search to include only components from actors of this type. This can provide a considerable speedup.
	 *                           Can be empty, in which case all actors are considered.
	 *  @param OutType   Defines the type of the output array. Note that this does not narrow the search;
	 *                   make sure that the found components actually fit this type!
	 *                   Can be empty, in which case the output array becomes an Actor Component array. */
	UFUNCTION( BlueprintCallable, Category = "Object Selector", meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "OutType", DynamicOutputParam = "Out") )
	static void GetAllMatchingComponentsInWorld( const FComponentSelector & ComponentSelector, const UObject * WorldContextObject,
		TSubclassOf<AActor> OwnerActorClass, TSubclassOf<UActorComponent> OutType, TArray<UActorComponent *> & Out );
};


UCLASS()
class UActorSelectorBlueprintHelpers : public UObject
{
	GENERATED_BODY()

public:

	/** Test whether an actor matches the selection criteria. */
	UFUNCTION( BlueprintCallable, BlueprintPure, Category = "Object Selector" )
	static bool IsMatchingActor( const FActorSelector & ActorSelector, const AActor * Actor );

	/** Filter an array by removing all elements that do not match the selection criteria. The array order of the remaining objects is not preserved. */
	UFUNCTION( BlueprintCallable, Category = "Object Selector" )
	static void FilterActorArray( const FActorSelector & ActorSelector, UPARAM( ref ) TArray<AActor *> & Array );

	/** Returns an array containing all matching actors in the world.
	 *  @param Class   Narrow the search to include only objects of this type. Also defines the type of the output array.
	 *                 Can be empty, in which case all actors are considered and the output array becomes an AActor array.
	 *                 Narrowing the search already here can provide a considerable speedup. */
	UFUNCTION( BlueprintCallable, Category = "Object Selector", meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "Class", DynamicOutputParam = "Out") )
	static void GetAllMatchingActors( const FActorSelector & ActorSelector, const UObject * WorldContextObject, TSubclassOf<AActor> Class, TArray<AActor *> & Out );
};








/* method dispatchers for methods that use the statically polymorphic template method pattern */




bool FComponentSelector::IsMatching( const UActorComponent & component ) const
{
	return FObjectSelector::IsMatching<FComponentSelector>( component );
}


bool FActorSelector::IsMatching( const AActor & actor ) const
{
	// IncludeByReference
	if( IncludeByReference.Contains( &actor ) ) return true;

	// no reference match, proceed with conditions in the base class
	return FObjectSelector::IsMatching<FActorSelector>( actor );
}


template<typename T>
TArray<T *> & FComponentSelector::FilterArray( TArray<T *> & array ) const
{
	return FObjectSelector::FilterArray<FComponentSelector>( array );
}


template<typename T>
TArray<T *> & FActorSelector::FilterArray( TArray<T *> & array ) const
{
	return FObjectSelector::FilterArray<FActorSelector>( array );
}








/* implementations for templated and/or inlined functions */




template<typename this_t, typename object_t>
bool FObjectSelector::IsMatching( const object_t & object ) const
{
	UE_LOG( LogTemp, Error, TEXT( "Object IsMatching: %s" ), *object.GetName() );

	// IncludeByName
	if( IncludeByName.Contains( object.GetFName() ) ) return true;

	// IncludeByTag
	for( const FName & tag : IncludeByTag )
	{
		if( HasTag( object, tag ) ) return true;
	}

	// IncludeByFilter:
	//   - for each filter
	//     - no match: continue with next filter
	//     - have match: return true (filters combine in an OR fashion, so we require only one filter to match)
	for( const auto & filter : statically_polymorphic_this<this_t>()->IncludeByFilter )
	{
		// NarrowByTag
		if( !filter.NarrowByTag.IsNone() && !HasTag( object, filter.NarrowByTag ) ) continue;

		// NarrowByClass
		if( filter.NarrowByClass && !object.IsA( filter.NarrowByClass ) ) continue;

		// NarrowByMobility: reject if not a USceneComponent (thus no mobility) or has a different mobility than required here
		if( filter.NarrowByMobility != EObjectSelectorMobilityFilter::Any )
		{
			if( OurMobilityToUEMobilityMap[static_cast<std::size_t>(filter.NarrowByMobility)] != GetMobility( object ) ) continue;
		}

		// NarrowByNamePattern
		if( !filter.NarrowByNamePattern.IsEmpty() && !FWildcardString( filter.NarrowByNamePattern ).IsMatch( object.GetName() ) ) continue;

		// NarrowByTagPattern
		if( !filter.NarrowByTagPattern.IsEmpty() )
		{
			bool found = false;
			for( const FName & tag : GetTags( object ) )
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




template<typename this_t, typename T>
TArray<T *> & FObjectSelector::FilterArray( TArray<T *> & array ) const
{
	// iterate over the array
	for( int32 i = 0; i < array.Num(); ++i )
	{
		// remove if null or not matching
		if( !array[i] || !statically_polymorphic_this<this_t>()->IsMatching( *array[i] ) )
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




template<typename T>
TArray<T *> FComponentSelector::GetAllMatchingComponents( UWorld & world, UClass * ownerActorClass/* = AActor::StaticClass()*/ ) const
{
	TArray<T *> result;

	// We could just use GetObjectsOfClass (or ForEachObjectOfClass) and filter the result, but we would have to be careful to filter the components properly
	// to avoid components from the editor world, from hidden levels, etc.
	// Play safe and avoid possible inconsistencies: do this by enumerating actors via the standard actor iterator
	for( TActorIterator<AActor> it( &world, ownerActorClass ? TSubclassOf<AActor>(ownerActorClass) : AActor::StaticClass() ); it; ++it )
	{
		result.Append( GetAllMatchingComponents( **it ) );
	}

	return result;
}




template<typename T>
TArray<T *> FActorSelector::GetAllMatchingActors( UWorld & world, UClass * uClass ) const
{
	TArray<T *> result;

	// We could use ForEachObjectOfClass() to avoid one temporary copy of the candidate actor list, but we would have to be careful to filter the
	// actors properly to avoid actors from the editor world, from hidden levels, etc.
	// Play safe and avoid possible inconsistencies: do this by enumerating actors via the standard actor iterator
	for( TActorIterator<AActor> it( &world, uClass ? TSubclassOf<T>(uClass) : AActor::StaticClass() ); it; ++it )
	{
		if( IsMatching( **it ) )
		{
			result.Add( *it );
		}
	}

	return result;
}




bool FObjectSelector::HasTag( const UActorComponent & component, const FName & tag ) const
{
	return component.ComponentHasTag( tag );
}


bool FObjectSelector::HasTag( const AActor & actor, const FName & tag ) const
{
	return actor.ActorHasTag( tag );
}


const TArray<FName> & FObjectSelector::GetTags( const UActorComponent & component ) const
{
	return component.ComponentTags;
}


const TArray<FName> & FObjectSelector::GetTags( const AActor & actor ) const
{
	return actor.Tags;
}


EComponentMobility::Type FObjectSelector::GetMobility( const UActorComponent & component ) const
{
	const USceneComponent * sceneComponent = dynamic_cast<const USceneComponent *>(&component);
	return sceneComponent ? sceneComponent->Mobility : static_cast<EComponentMobility::Type>(0xff);
}


EComponentMobility::Type FObjectSelector::GetMobility( const AActor & actor ) const
{
	UActorComponent * rootComponent = actor.GetRootComponent();
	return rootComponent ? GetMobility( *rootComponent ) : static_cast<EComponentMobility::Type>(0xff);
}
