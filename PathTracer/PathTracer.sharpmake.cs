using Sharpmake;

namespace Fancy
{
  [Sharpmake.Generate]
  public class PathTracerProject : FancyExternalApplication
  {
    public PathTracerProject()
    {
      Name = "PathTracer";
    }

    [Sharpmake.Configure]
    public override void ConfigureAll(Configuration conf, Target target)
    {
      base.ConfigureAll(conf, target);
    }
  }
}

