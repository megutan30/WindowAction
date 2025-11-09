using System;

namespace MultiWindowActionGame.DI
{
    public interface IServiceContainer
    {
        void RegisterSingleton<TInterface, TImplementation>()
            where TImplementation : class, TInterface;

        void RegisterSingleton<T>(T instance)
            where T : class;

        void RegisterTransient<TInterface, TImplementation>()
            where TImplementation : class, TInterface;

        T Resolve<T>();

        object Resolve(Type serviceType);

        bool IsRegistered<T>();

        bool IsRegistered(Type serviceType);
    }
}