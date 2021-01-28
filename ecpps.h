#ifndef ECS_H
#define ECS_H

#include <vector>
#include <map>
#include <iostream>
#include <typeinfo>
#include <any>
#include <memory>
#include <SDL2/SDL.h>

using std::vector;
using std::map;
using std::any;
using std::type_info;
using std::is_base_of;

// going to try to keep this constrained to one file
// the goal of this system is to provide an OOP-style interface
// while maintaining DDP-style performance undernearth

namespace ecpps {
typedef unsigned ID;
class ECSManager;
class ComponentManager;

// ####### Class definitions ####### //

// wrapper class, entity is really just an ID but this gives OOP approach to management. serves as a reference point for components
class Entity {
    private:
        // reference to manager for when adding component
        ECSManager& manager;
        // unique ID for every entity
        ID entityID;
    public:
        // constructor
        Entity(ID entityID, ECSManager* manager) : entityID(entityID),manager(*manager){};
        // adds component to manager with entity id
        template <typename T> inline void addComponent(T component);
        // used to destroy object for all component managers and delete self from manager
        inline void destroy();
};

// base struct for components, meant to be dervied and then added to entities
struct Component {
};

// base struct for components designed for rendering
struct RenderComponent : public Component {
        SDL_Texture* renderTexture;
};

// class for maintaining component vector and entity indexes
class IComponentVector {
    private:
    public:
        virtual void removeEntity(ID entityID)=0;
};

// class for maintaining component vector and entity indexes
template <typename T>
class ComponentVector : public IComponentVector {
    private:
        // holds indexes of entityID
        map<ID, unsigned> indexes;
        // holds vector of components
        vector<T> components;
    public:
        inline void addComponent(ID entityID, T component);
        vector<T>& getComponentVector() { return components; };
        inline void removeEntity(ID entityID) override;
        inline T& getComponentAt(ID entityID);
};

// manages component vectors and tosses around pointers like it's nothing
class ComponentManager {
    private:
        map<const char*, std::shared_ptr<IComponentVector>> componentVectors;
    public:
        template <typename T> void addComponent(ID entityID, T component);
        template <typename T> std::shared_ptr<ComponentVector<T>> getComponentVector();
        inline void removeEntity(ID entityID);
};

// base class for systems, fed vectors of components and then perform operations on them
class System {
    private:
    public:
        virtual void init() {};
        virtual void update(ECSManager* manager) {};
};

// base class for systems used for rendering
class RenderSystem : public System {
    private:
    public:
        virtual void render(ECSManager* manager) {};
};

// holds much of the top level ECS data and functionality
class ECSManager {
    private:
        // holds all systems for looping through updates
        vector<System> systems;
        // holds all render systems for looping through renders
        vector<RenderSystem> rsystems;
        // holds all component vectors
        ComponentManager components;
        // holds all Entity class objects (must be kept around until ready to delete)
        map<ID, Entity> entities;
        // holds current value to generate new entityIDs
        ID nextID = 0;
        // holds all IDs that once belonged to entities that have been deleted (reusable)
        vector<ID> reusableIDs;
        // creates a unique ID for each enitity
        inline ID generateEntityID();
    public:
        ECSManager() {};
        // creates a default entity
        inline Entity& createEntity();
        // destroys an entity
        inline void destroyEntity(ID entityID);
        // adds a component of any type to a database of that type
        template <typename T> inline void addComponent(ID entityID, T component);
        // returns Component Vector pointer for systems
        template <typename T> std::shared_ptr<ComponentVector<T>> getComponentVector();
        // registers a new system
        template <typename T> inline void registerSystem();
        // updates all systems
        inline void update();
        // renders all rendersystems
        inline void render();
};

// most often used ECSManager type, usually near the root of the program
class Scene: public ECSManager {
    public:
        Scene() {};
};

// ####### Everything else ####### //

// ------- Entity ------- //

template <typename T>
void Entity::addComponent(T component){
    // check and see if object is derived from Component
    if(is_base_of<Component,T>::value == 1){
        // send component to manager
        manager.addComponent<T>(entityID, component);
    }
}

void Entity::destroy(){
    // tell manager to destroy enemy
    manager.destroyEntity(entityID);
}

// ------- ComponentManager ------- //

template <typename T>
void ComponentVector<T>::addComponent(ID entityID, T component) {
    // define index
    unsigned index;
    // if empty
    if(components.empty()){
        // set to 0
       index = 0;
        // else if populated
    } else {
        // get vector size (will be the same as getting it later and subtracting 1)
        index = components.size();
    }
    
    // get entity index in internal map
    indexes.insert({entityID, index});
    // place component in vector
    components.emplace_back(component);
    
    std::cout << " -------- adding component to id: " << entityID << std::endl;
    std::cout << "typename: " << typeid(T).name() << std::endl;
    std::cout << "current entity: " << entityID << " at index: " << index << std::endl;
    std::cout << "existing component indexes" << std::endl;
    for (const auto& [key, value]: indexes) {
        std::cout << "key: " << key << ", value: " << value << std::endl;
    }
}

template <typename T>
void ComponentVector<T>::removeEntity(ID entityID) {
    // get index of entity
    unsigned index = indexes.at(entityID);
    // remove index from map
    indexes.erase(entityID);
    // remove entity from vector
    components.erase(components.begin() + index);
    // update all indexes
    std::cout << " -------- removing entity with id: " << entityID << std::endl;
    std::cout << "typename: " << typeid(T).name() << std::endl;
    std::cout << "current entity: " << entityID << " at index: " << index << std::endl;
    std::cout << "changing all other component indexes" << std::endl;
    for (const auto& [key, value]: indexes) {
        std::cout << "key: " << key << ", value: " << value << std::endl;
        // if stored index of other entity is higher than the index of the deleted entity
        if(value > index){
            // then subtract by 1
            indexes[key] = value - 1;
            std::cout << "key: " << key << ", new value: " << value << std::endl;
        } else {
            std::cout << "retained old value" << std::endl;
        }
    }
}

template <typename T>
inline T& ComponentVector<T>::getComponentAt(ID entityID) {
    // get index of entity
    unsigned index = indexes[entityID];
    // return component at index
    return components.at(index);
}

template <typename T>
void ComponentManager::addComponent(ID entityID, T component){
    // first, get type_info to check
    const char* typekey = typeid(T).name();

    // this is where the pointer-magic happens
    // get pointer for type, then send new component data
    getComponentVector<T>()->addComponent(entityID, component);
}

template <typename T>
std::shared_ptr<ComponentVector<T>> ComponentManager::getComponentVector(){
    // first, get type_info to check
    const char* typekey = typeid(T).name();

    // check if map entry exists
    if(componentVectors.find(typekey) == componentVectors.end()){
        //if not, create one
        componentVectors.insert({typekey, std::make_shared<ComponentVector<T>>()});
    }

    // return pointer for component vector
    return std::static_pointer_cast<ComponentVector<T>>(componentVectors.at(typekey));
}

void ComponentManager::removeEntity(ID entityID){
    for(const auto& [key, value] : componentVectors){
        auto& component = value;
        
        component -> removeEntity(entityID);
    }
}

// ------- ECSManager ------- //

Entity& ECSManager::createEntity(){
    // get unique ID
    ID newID = generateEntityID();
    // create entity with id and reference to manager
    Entity newEntity(newID, this);

    std::cout << " -------- creating entity at id: " << newID << std::endl;

    // add entity to vector
    entities.insert({newID, newEntity});
    // return reference to entity
    return entities.at(newID);
}

// destroys an entity
void ECSManager::destroyEntity(ID entityID) {
    // get entity from map
    Entity& toDestroy = entities.at(entityID);

    std::cout << " -------- deleting entity at id: " << entityID << std::endl;

    // remove entity from components
    components.removeEntity(entityID);
    // erase entity from id map
    entities.erase(entityID);
    // add entity id to reclaimable id list
    reusableIDs.emplace_back(entityID);
}

template <typename T>
void ECSManager::addComponent(ID entityID, T component){ 
    // check and see if object is derived from Component
    if(is_base_of<Component,T>::value == 1){
        // pass to component manager
        components.addComponent<T>(entityID, component);
    }
}

template <typename T>
std::shared_ptr<ComponentVector<T>> ECSManager::getComponentVector(){ 
    // pass return from components
    return components.getComponentVector<T>();
}

template <typename T>
void ECSManager::registerSystem(){
    // check if system
    if constexpr (is_base_of<System,T>::value == 1){
        // check if render system
        if constexpr (is_base_of<RenderSystem,T>::value == 1){
            // create render system
            T rsystem;
            // add to vector
            rsystems.emplace_back(rsystem);
            // init

        }
        // create system
        T system;
        // add to vector
        systems.emplace_back(system);
        // init
        system.init();
    }
}

ID ECSManager::generateEntityID(){
    // prepare ID for return
    ID toReturn;
    // if available reusable ids
    if(reusableIDs.size() != 0){
        // pull the one from the end
        toReturn = reusableIDs.back();
        // remove the one from the end
        reusableIDs.pop_back();
    // if no reusable ids
    } else {
        // return current id pointer
        toReturn = nextID;
        // iterate id pointer
        nextID++;
    }
    //return ID
    return toReturn;
}

void ECSManager::update(){
    // update all systems
    for(System& system : systems){
        system.update(this);
    }

    // after much internal debate, i decided that render systems should get their own update
    // ideally, render and update code are kept in seperate systems
    // but if a render system **must** update, shouldn't it be seperated at least from the render function?
    // although, loading every rendersystem into memory twice each gameloop might not be worth
    // it really depends on wether or not rendersystems ever need updating, which needs further testing
    // if I find that they don't and can be seperated easily, then i'll remove this

    // update all render systems
    for(RenderSystem& rsystem : rsystems){
        rsystem.update(this);
    }
}

void ECSManager::render(){
    // render all render systems
    for(RenderSystem& rsystem : rsystems){
        rsystem.render(this);
    }
}

};
#endif // ECS_H