/*
 * Copyright 2014 Nicola Gigante
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OCMESH_object_H__
#define OCMESH_object_H__

#include "glm.h"
#include "voxel.h"

#include <std14/memory>
#include <iterator>
#include <istream>
#include <vector>
#include <deque>

namespace ocmesh {

namespace details {
    
    class scene;
    
    /*
     * Root class of the CSG nodes hierarchy
     */
    class object
    {
        scene *_scene;
    public:
        object(scene *s) : _scene(s) { }
        
        virtual ~object();
        
        class scene *scene() const { return _scene; }
        
        virtual float distance(glm::vec3 const& from) = 0;
        virtual void dump(std::ostream &) const = 0;
    };

    /*
     * Basic primitives
     */
    class sphere_t : public object
    {
        float _radius;
        
    public:
        sphere_t(class scene *s, float radius)
            : object(s), _radius(radius) { }
        
        float radius() const { return _radius; }
        
        float distance(glm::vec3 const& from) override;
        void dump(std::ostream &) const override;
    };
    
    class cube_t : public object
    {
        float _side;
        
    public:
        cube_t(class scene *s, float side)
            : object(s), _side(side) { }
        
        float side() const { return _side; }
        
        float distance(glm::vec3 const& from) override;
        void dump(std::ostream &) const override;
    };
    
    /*
     * Node for toplevel objects in the scene
     */
    class toplevel_t : public object
    {
        object *_child;
        voxel::material_t _material;

    public:
        toplevel_t(class scene *s, object *child, voxel::material_t material)
            : object(s), _child(child), _material(material) { }
        
        object *child() const {  return _child; }
        
        voxel::material_t material() const { return _material; }
        
        float distance(glm::vec3 const& from) override;
        void dump(std::ostream &) const override;
    };
    
    /*
     * Class that owns all the objects of the scene
     */
    class scene
    {
        template<typename T>
        using container_t = std::deque<T>;
        
    public:
        using value_type     = object *;
        using const_iterator = container_t<toplevel_t *>::const_iterator;
        using iterator       = const_iterator;
        
    public:
        scene() = default;
        scene(scene const&) = delete;
        scene(scene     &&) = default;
        
        scene &operator=(scene const&) = delete;
        scene &operator=(scene     &&) = default;
        
        /*
         * Get the global objects of the scene
         */
        const_iterator cbegin() const { return _toplevels.cbegin(); }
        const_iterator cend()   const { return _toplevels.cend();   }
        
        const_iterator begin()  const { return cbegin(); }
        const_iterator end()    const { return cend();   }

        size_t size() const { return _toplevels.size(); }
        
        void toplevel(object *obj, voxel::material_t material) {
            _toplevels.push_back(make<toplevel_t>(obj, material));
        }
        
        class parse_result;
        
        /*
         * Function to fill the scene by parsing an input file
         */
        parse_result parse(std::istream &);
        
        /*
         * Primitives
         */
        object *sphere(float radius) {
            return make<sphere_t>(radius);
        }
        
        object *cube(float side) {
            return make<cube_t>(side);
        }
        
        friend std::ostream &operator<<(std::ostream &s, scene const&scene) {
            for(auto t : scene._toplevels) {
                t->dump(s);
                s << "\n";
            }
            return s;
        }
        
    private:
        template<typename T, typename ...Args,
                 REQUIRES(std::is_constructible<T, scene *, Args...>())>
        T *make(Args&& ...args)
        {
            auto p = std14::make_unique<T>(this, std::forward<Args>(args)...);
            T *r = p.get();
            _objects.push_back(std::move(p));
            return r;
        }
        
        /*
         * Binary operations friend declarations
         */
        friend
        object *unite(object *left, object *right);
        
        friend
        object *intersect(object *left, object *right);
        
        friend
        object *subtract(object *left, object *right);
        
        friend
        object *transform(object *node, glm::mat4 const&matrix);
        
    private:
        container_t<std::unique_ptr<object>> _objects;
        container_t<toplevel_t *> _toplevels;
    };

    class scene::parse_result
    {
        bool _ok = true;
        std::string _error;
        
    public:
        parse_result() = default;
        
        parse_result(bool ok, std::string error)
            : _ok(ok), _error(std::move(error)) { }
        
        bool ok() const { return _ok; }
        
        explicit operator bool() const { return _ok; }
    
        std::string const&error() const { return _error; }
    };
    
    /*
     * Classes representing CSG nodes
     */
    class binary_operation_t : public object
    {
        object *_left;
        object *_right;
        
    public:
        binary_operation_t(class scene *scene, object *left, object *right)
        : object(scene), _left(left), _right(right) { }
        
        virtual ~binary_operation_t();
        
        object *left()  const { return _left;  }
        object *right() const { return _right; }
    };
    
    class union_t : public binary_operation_t {
    public:
        using binary_operation_t::binary_operation_t;
        
        float distance(glm::vec3 const& from) override;
        void dump(std::ostream &) const override;
    };
    
    class intersection_t : public binary_operation_t {
    public:
        using binary_operation_t::binary_operation_t;
        
        float distance(glm::vec3 const& from) override;
        void dump(std::ostream &) const override;
    };
    
    class difference_t : public binary_operation_t {
    public:
        using binary_operation_t::binary_operation_t;
        
        float distance(glm::vec3 const& from) override;
        void dump(std::ostream &) const override;
    };
    
    class transform_t : public object
    {
        object *_child;
        glm::mat4 _transform;
        
    public:
        transform_t(class scene *scene,
                    object *child, glm::mat4 const&transform)
        : object(scene), _child(child), _transform(transform) { }
        
        object *child() const { return _child; }
        glm::mat4 const&transform() const { return _transform; }
        
        float distance(glm::vec3 const& from) override;
        void dump(std::ostream &) const override;
    };
    
    /*
     * Binary operations
     */
    inline object *unite(object *left, object *right) {
        assert(left->scene() == right->scene());
        
        return left->scene()->make<union_t>(left, right);
    }
    
    inline object *intersect(object *left, object *right) {
        assert(left->scene() == right->scene());
        
        return left->scene()->make<intersection_t>(left, right);
    }
    
    template<typename ...Args>
    inline object *unite(object *node, Args&& ...args) {
        return unite(std::move(node), unite(std::forward<Args>(args)...));
    }
    
    inline object *subtract(object *left, object *right) {
        assert(left->scene() == right->scene());
        
        return left->scene()->make<difference_t>(left, right);
    }
    
    /*
     * Transform
     * Use this function directly only if you know what you're doing.
     * Use scale/rotate/translate instead.
     * The matrix must be a transform from object to world space
     */
    inline object *transform(object *node, glm::mat4 const&matrix) {
        return node->scene()->make<transform_t>(node, matrix);
    }
    
    /*
     * Convenience functions to call the above ones
     */
    template<typename ...Args>
    inline object *intersect(object *node, Args&& ...args) {
        return intersect(std::move(node),
                         unite(std::forward<Args>(args)...));
    }
    
    
    
    template<typename ...Args>
    inline object *subtract(object *node, Args&& ...args) {
        return subtract(std::move(node),
                        unite(std::forward<Args>(args)...));
    }
    
    inline object *scale(object *node, glm::vec3 const&factors) {
        assert(glm::all(glm::notEqual(factors, {})) &&
               "Scaling factor components must be non-zero");
        return transform(std::move(node),
                         glm::scale(glm::vec3(1, 1, 1) / factors));
    }
    
    inline object *scale(object *node, float factor) {
        assert(factor != 0 && "Scaling factor must be non-zero");
        return scale(std::move(node), { factor, factor, factor });
    }
    
    inline object *xscale(object *node, float factor) {
        assert(factor != 0 && "Scaling factor must be non-zero");
        return scale(std::move(node), {factor, 1, 1});
    }
    
    inline object *yscale(object *node, float factor) {
        assert(factor != 0 && "Scaling factor must be non-zero");
        return scale(std::move(node), {1, factor, 1});
    }
    
    inline object *zscale(object *node, float factor) {
        assert(factor != 0 && "Scaling factor must be non-zero");
        return scale(std::move(node), {1, 1, factor});
    }
    
    inline object *rotate(object *node, float angle, glm::vec3 const&axis) {
        return transform(std::move(node), glm::rotate(-angle, axis));
    }
    
    inline object *xrotate(object *node, float angle) {
        return rotate(std::move(node), angle, {1, 0, 0});
    }
    
    inline object *yrotate(object *node, float angle) {
        return rotate(std::move(node), angle, {0, 1, 0});
    }
    
    inline object *zrotate(object *node, float angle) {
        return rotate(std::move(node), angle, {0, 0, 1});
    }
    
    inline object *translate(object *node, glm::vec3 const&offsets) {
        return transform(std::move(node), glm::translate(-offsets));
    }
    
    inline object *xtranslate(object *node, float offset) {
        return translate(std::move(node), {offset, 0, 0});
    }
    
    inline object *ytranslate(object *node, float offset) {
        return translate(std::move(node), {0, offset, 0});
    }
    
    inline object *ztranslate(object *node, float offset) {
        return translate(std::move(node), {0, 0, offset});
    }
} // namespace details

namespace csg {
    using details::object;
    using details::scene;
    
    using details::unite;
    using details::intersect;
    using details::subtract;
    using details::transform;
    
    using details::scale;
    using details::xscale;
    using details::yscale;
    using details::zscale;
    
    using details::rotate;
    using details::xrotate;
    using details::yrotate;
    using details::zrotate;
    
    using details::translate;
    using details::xtranslate;
    using details::ytranslate;
    using details::ztranslate;
    
} // namespace csg
} // namespace ocmesh

#endif
