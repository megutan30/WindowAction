using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;

namespace MultiWindowActionGame.DI
{
    public enum ServiceLifetime
    {
        Singleton,
        Transient
    }

    public class ServiceDescriptor
    {
        public Type ServiceType { get; set; } = typeof(object);
        public Type ImplementationType { get; set; } = typeof(object);
        public ServiceLifetime Lifetime { get; set; }
        public object? Instance { get; set; }
        public Func<IServiceContainer, object>? Factory { get; set; }
    }

    public class ServiceContainer : IServiceContainer
    {
        private readonly Dictionary<Type, ServiceDescriptor> services = new();
        private readonly ConcurrentDictionary<Type, object> singletonInstances = new();
        private readonly HashSet<Type> circularDependencyCheck = new();

        public void RegisterSingleton<TInterface, TImplementation>()
            where TImplementation : class, TInterface
        {
            services[typeof(TInterface)] = new ServiceDescriptor
            {
                ServiceType = typeof(TInterface),
                ImplementationType = typeof(TImplementation),
                Lifetime = ServiceLifetime.Singleton
            };
        }

        public void RegisterSingleton<T>(T instance) where T : class
        {
            services[typeof(T)] = new ServiceDescriptor
            {
                ServiceType = typeof(T),
                ImplementationType = typeof(T),
                Lifetime = ServiceLifetime.Singleton,
                Instance = instance
            };
            singletonInstances[typeof(T)] = instance;
        }

        public void RegisterTransient<TInterface, TImplementation>()
            where TImplementation : class, TInterface
        {
            services[typeof(TInterface)] = new ServiceDescriptor
            {
                ServiceType = typeof(TInterface),
                ImplementationType = typeof(TImplementation),
                Lifetime = ServiceLifetime.Transient
            };
        }

        public void RegisterFactory<T>(Func<IServiceContainer, T> factory, ServiceLifetime lifetime = ServiceLifetime.Singleton)
        {
            services[typeof(T)] = new ServiceDescriptor
            {
                ServiceType = typeof(T),
                ImplementationType = typeof(T),
                Factory = container => factory(container) ?? throw new InvalidOperationException($"Factory for {typeof(T).Name} returned null"),
                Lifetime = lifetime
            };
        }

        public void RegisterConditional<T>(T instance, Func<bool> condition) where T : class
        {
            var descriptor = new ServiceDescriptor
            {
                ServiceType = typeof(T),
                ImplementationType = typeof(T),
                Lifetime = ServiceLifetime.Singleton,
                Instance = instance
            };

            if (condition())
            {
                services[typeof(T)] = descriptor;
                singletonInstances[typeof(T)] = instance;
            }
        }

        public T Resolve<T>()
        {
            return (T)Resolve(typeof(T));
        }

        public object Resolve(Type serviceType)
        {
            if (!services.TryGetValue(serviceType, out var registration))
            {
                throw new InvalidOperationException($"Service of type {serviceType.Name} is not registered.");
            }

            // 循環依存チェック
            if (circularDependencyCheck.Contains(serviceType))
            {
                throw new InvalidOperationException($"Circular dependency detected for service type {serviceType.Name}");
            }

            if (registration.Lifetime == ServiceLifetime.Singleton)
            {
                if (registration.Instance != null)
                {
                    return registration.Instance;
                }

                return singletonInstances.GetOrAdd(serviceType, _ =>
                {
                    if (registration.Factory != null)
                    {
                        circularDependencyCheck.Add(serviceType);
                        try
                        {
                            return registration.Factory(this);
                        }
                        finally
                        {
                            circularDependencyCheck.Remove(serviceType);
                        }
                    }
                    return CreateInstance(registration.ImplementationType);
                });
            }

            if (registration.Factory != null)
            {
                circularDependencyCheck.Add(serviceType);
                try
                {
                    return registration.Factory(this);
                }
                finally
                {
                    circularDependencyCheck.Remove(serviceType);
                }
            }

            return CreateInstance(registration.ImplementationType);
        }

        public bool IsRegistered<T>()
        {
            return IsRegistered(typeof(T));
        }

        public bool IsRegistered(Type serviceType)
        {
            return services.ContainsKey(serviceType);
        }

        private object CreateInstance(Type type)
        {
            circularDependencyCheck.Add(type);
            try
            {
                var constructors = type.GetConstructors();
                var constructor = constructors.OrderByDescending(c => c.GetParameters().Length).First();

                var parameters = constructor.GetParameters();
                var args = new object[parameters.Length];

                for (int i = 0; i < parameters.Length; i++)
                {
                    var parameterType = parameters[i].ParameterType;
                    if (IsRegistered(parameterType))
                    {
                        args[i] = Resolve(parameterType);
                    }
                    else
                    {
                        throw new InvalidOperationException(
                            $"Cannot resolve parameter {parameters[i].Name} of type {parameterType.Name} for {type.Name}. " +
                            "Make sure all dependencies are registered.");
                    }
                }

                return Activator.CreateInstance(type, args)
                    ?? throw new InvalidOperationException($"Failed to create instance of {type.Name}");
            }
            finally
            {
                circularDependencyCheck.Remove(type);
            }
        }
    }
}